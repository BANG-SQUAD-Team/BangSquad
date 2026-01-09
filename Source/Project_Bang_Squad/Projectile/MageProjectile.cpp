#include "Project_Bang_Squad/Projectile/MageProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Particles/ParticleSystemComponent.h"

AMageProjectile::AMageProjectile()
{
    bReplicates = true;
    SetReplicateMovement(true);

    // 1. 구체 충돌체 설정
    SphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
    SphereComp->InitSphereRadius(15.0f);
    
    // [핵심 변경] 충돌 프로필: 일단 다 막고(Block), 필요한 것만 뚫기
    SphereComp->SetCollisionProfileName(TEXT("Custom")); // 커스텀 설정 시작
    
    SphereComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); // 물리 충돌(Block)도 가능하게 변경
    SphereComp->SetCollisionResponseToAllChannels(ECR_Block);            // 기본: 다 막힘 (벽, 바닥 등)
    SphereComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);    // 적(Pawn): 뚫고 지나가며 Overlap 발생
    SphereComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);   // 카메라: 무시
    
    SphereComp->SetGenerateOverlapEvents(true);
    RootComponent = SphereComp;

    // 2. 외형 메시
    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(RootComponent);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);

    // 3. 파티클
    ParticleComp = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("ParticleComp"));
    ParticleComp->SetupAttachment(RootComponent);

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

// [상황 1] 적(Pawn)과 겹쳤을 때 -> 데미지 주고 삭제
void AMageProjectile::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
                                UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor || OtherActor == this) return;
    if (GetOwner() && OtherActor == GetOwner()) return; // 주인 무시

    if (HasAuthority())
    {
        // Pawn(몬스터)인지 확인 (벽은 여기서 처리 안 함)
        if (OtherActor->IsA(APawn::StaticClass())) 
        {
            // 데미지 전달
            UGameplayStatics::ApplyDamage(OtherActor, Damage, GetInstigatorController(), this, UDamageType::StaticClass());
            
            // 데미지 주고 즉시 삭제
            Destroy();
        }
    }
}

// [추가] [상황 2] 벽이나 바닥 등 단단한 물체(Block)에 부딪혔을 때 -> 그냥 삭제
void AMageProjectile::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
    Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

    if (HasAuthority())
    {
        // 주인이나 나 자신이 아니면 파괴 (벽에 부딪힘)
        if (Other && (Other != this) && (Other != GetOwner()))
        {
            // (옵션) 여기서 벽에 부딪히는 이펙트(Sparks)를 스폰하면 좋습니다.
            // UGameplayStatics::SpawnEmitterAtLocation(...)
            
            Destroy();
        }
    }
}