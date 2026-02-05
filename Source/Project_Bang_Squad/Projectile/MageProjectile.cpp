#include "Project_Bang_Squad/Projectile/MageProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h" 
#include "NiagaraComponent.h"
#include "Net/UnrealNetwork.h"
#include "Particles/ParticleSystemComponent.h"

AMageProjectile::AMageProjectile()
{
    bReplicates = true;
    SetReplicateMovement(true);

    // 1. 구체 충돌체 설정
    SphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
    SphereComp->InitSphereRadius(15.0f);

    //  프로필 이름을 쓰지 않고 직접 채널을 설정합니다.
    // SphereComp->SetCollisionProfileName(TEXT("Custom")); 

    SphereComp->SetCollisionObjectType(ECC_GameTraceChannel1);
    
    // -------------------------------------------------------------------------
    //  충돌 전략 변경: "일단 무시(Ignore)하고 필요한 것만 설정ㄴ
    // -------------------------------------------------------------------------
    SphereComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    
    // 2. 벽, 바닥 등 뚫으면 안 되는 정적 물체는 막음 (Block -> NotifyHit 호출됨)
    SphereComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    SphereComp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block); // 움직이는 문/플랫폼 등
    SphereComp->SetCollisionResponseToChannel(ECC_Destructible, ECR_Block);

    // 3. 캐릭터(Pawn), 몬스터, 그리고 **캐릭터의 메시(PhysicsBody)**는 뚫고 지나감 (Overlap -> OnOverlap 호출됨)
    SphereComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);        // 캡슐 컴포넌트
    SphereComp->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap); // [중요] 캐릭터의 팔다리(Mesh)
    SphereComp->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);

    // 4. 카메라와 시야 체크용 채널은 무시
    SphereComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    SphereComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
    // -------------------------------------------------------------------------

    SphereComp->SetGenerateOverlapEvents(true);
    RootComponent = SphereComp;

    // 2. 외형 메시
    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(RootComponent);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);

    // 3. 나이아가라
    NiagaraComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraComp"));
    NiagaraComp->SetupAttachment(RootComponent);

    // 4. 이동 설정
    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->UpdatedComponent = SphereComp;
    ProjectileMovement->InitialSpeed = 2000.f;
    ProjectileMovement->MaxSpeed = 2000.f;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->ProjectileGravityScale = 0.f; 

    // 충돌 함수 연결
    SphereComp->OnComponentBeginOverlap.AddDynamic(this, &AMageProjectile::OnOverlap);
    
    InitialLifeSpan = 3.0f;
}

void AMageProjectile::BeginPlay()
{
    Super::BeginPlay();

    // 주인(플레이어) 무시 설정
    if (GetOwner())
    {
        SphereComp->IgnoreActorWhenMoving(GetOwner(), true);
        if (UPrimitiveComponent* OwnerRoot = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()))
        {
            SphereComp->IgnoreComponentWhenMoving(OwnerRoot, true);
        }
    }
}

// 적(Pawn)과 겹쳤을 때 -> 데미지 주고 삭제
void AMageProjectile::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
                                UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor || OtherActor == this) return;
    if (GetOwner() && OtherActor == GetOwner()) return; // 주인 무시

    if (OtherActor->ActorHasTag(TEXT("Player")))
    {
        return;
    }
    
    if (HasAuthority())
    {
        // Pawn(몬스터)인지 확인 (벽은 여기서 처리 안 함)
        if (OtherActor) 
        {
            // 데미지 전달
            UGameplayStatics::ApplyDamage(OtherActor, Damage, GetInstigatorController(), this, UDamageType::StaticClass());
            
            Multicast_SpawnHitVFX(GetActorLocation(), FRotator::ZeroRotator);
            
            SetLifeSpan(0.1f);
        }
    }
}

//  벽이나 바닥 등 단단한 물체(Block)에 부딪혔을 때 -> 그냥 삭제
void AMageProjectile::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
    Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

    if (HasAuthority())
    {
        bool bIsAlly = Other && Other->ActorHasTag(TEXT("Player"));
        
        // 주인이나 나 자신이 아니면 파괴 (벽에 부딪힘)
        if (Other && (Other != this) && (Other != GetOwner()) && !bIsAlly)
        {
            // 부딪힌 대상에게도 데미지를 줘야함
            UGameplayStatics::ApplyDamage(Other,Damage, GetInstigatorController(),
                this, UDamageType::StaticClass());
            
            Multicast_SpawnHitVFX(HitLocation, HitNormal.Rotation());
            
            SetLifeSpan(0.1f);
        }
    }
}

void AMageProjectile::Multicast_SpawnHitVFX_Implementation(FVector Location, FRotator Rotation)
{
    
    if (FireImpactVFX) 
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(),
            FireImpactVFX,
            Location,
            Rotation
        );
    }
    // 2. 즉시 삭제하지 말고, 안 보이게만 숨김 (가짜 죽음)
    if (MeshComp) MeshComp->SetVisibility(false);
    if (NiagaraComp) NiagaraComp->SetVisibility(false);
    
    // 3. 충돌 끄기 (더 이상 안 맞게)
    if (SphereComp) SphereComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // // 4. 이동 멈추기
    if (ProjectileMovement) ProjectileMovement->StopMovementImmediately();
}