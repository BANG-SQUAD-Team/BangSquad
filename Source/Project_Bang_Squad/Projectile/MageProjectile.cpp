#include "Project_Bang_Squad/Projectile/MageProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h" 
#include "NiagaraComponent.h"
#include "Net/UnrealNetwork.h"
#include "Particles/ParticleSystemComponent.h"

// ====================================================================================
// [Section 1] 생성자 (초기화 및 컴포넌트 조립)
// ====================================================================================

AMageProjectile::AMageProjectile()
{
    // 네트워크 동기화 설정
    bReplicates = true;
    SetReplicateMovement(true);

    // --- 1. 충돌 컴포넌트 (Root) 설정 ---
    SphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
    SphereComp->InitSphereRadius(15.0f);
    SphereComp->SetCollisionObjectType(ECC_GameTraceChannel1);
    
    // [충돌 전략] 일단 모두 무시(Ignore) 후, 필요한 채널만 세팅
    SphereComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    
    // 정적 물체(벽, 바닥)는 통과하지 못하고 부딪힘 -> NotifyHit 호출
    SphereComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    SphereComp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block); 
    SphereComp->SetCollisionResponseToChannel(ECC_Destructible, ECR_Block);

    // 동적 생명체(캐릭터 캡슐, 캐릭터 메쉬)는 겹침 판정 -> OnOverlap 호출
    SphereComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);        
    SphereComp->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap); 
    SphereComp->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);

    // 시야(카메라, 레이저) 검사용 채널은 무시하여 조준을 방해하지 않음
    SphereComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    SphereComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);

    SphereComp->SetGenerateOverlapEvents(true);
    RootComponent = SphereComp;

    // --- 2. 외형 (Mesh) 컴포넌트 ---
    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(RootComponent);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComp->SetCollisionResponseToAllChannels(ECR_Ignore); // 메쉬는 충돌 연산 제외

    // --- 3. 나이아가라 (VFX) 컴포넌트 ---
    NiagaraComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraComp"));
    NiagaraComp->SetupAttachment(RootComponent);

    // --- 4. 발사체 이동 (Movement) 컴포넌트 ---
    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->UpdatedComponent = SphereComp;
    ProjectileMovement->InitialSpeed = 2000.f;
    ProjectileMovement->MaxSpeed = 2000.f;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->ProjectileGravityScale = 0.f; // 직사 화기(마법)이므로 중력 0

    // --- 5. 델리게이트 및 라이프스팬 ---
    SphereComp->OnComponentBeginOverlap.AddDynamic(this, &AMageProjectile::OnOverlap);
    
    // 어딘가에 부딪히지 않아도 3초 뒤 자동 소멸
    InitialLifeSpan = 3.0f;
}

// ====================================================================================
// [Section 2] 라이프사이클 이벤트
// ====================================================================================

void AMageProjectile::BeginPlay()
{
    Super::BeginPlay();

    // 주인이 쏜 마법에 주인이 맞는 것을 방지하기 위한 예외 처리
    if (GetOwner())
    {
        SphereComp->IgnoreActorWhenMoving(GetOwner(), true);
        if (UPrimitiveComponent* OwnerRoot = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()))
        {
            SphereComp->IgnoreComponentWhenMoving(OwnerRoot, true);
        }
    }
}

// ====================================================================================
// [Section 3] 충돌 처리 (Overlap & Block)
// ====================================================================================

void AMageProjectile::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
                                UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // 유효성 및 본인/주인 예외 처리
    if (!OtherActor || OtherActor == this) return;
    if (GetOwner() && OtherActor == GetOwner()) return; 

    // 아군(Player)은 데미지 없이 통과
    static const FName PlayerTag = TEXT("Player");
    if (OtherActor->ActorHasTag(PlayerTag)) return;
    
    // 서버에서만 데미지 연산 수행
    if (HasAuthority())
    {
        AController* InstigatorController = GetInstigator() ? GetInstigator()->GetController() : nullptr;
        
        // 대상에게 데미지 전달
        UGameplayStatics::ApplyDamage(OtherActor, Damage, InstigatorController, this, UDamageType::StaticClass());
        
        // 피격 이펙트 재생 요청
        Multicast_SpawnHitVFX(GetActorLocation(), FRotator::ZeroRotator);
        
        // 이펙트 재생 시간을 벌기 위해 즉시 파괴하지 않고 0.1초 뒤 지연 파괴
        SetLifeSpan(0.1f);
    }
}

void AMageProjectile::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
    Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

    // 서버에서만 타격 판정 수행
    if (HasAuthority())
    {
        static const FName PlayerTag(TEXT("Player"));
        bool bIsAlly = Other && Other->ActorHasTag(PlayerTag);
        
        // 벽, 바닥, 또는 아군이 아닌 대상에 부딪혔을 때
        if (Other && (Other != this) && (Other != GetOwner()) && !bIsAlly)
        {
            AController* InstigatorController = GetInstigator() ? GetInstigator()->GetController() : nullptr;
            
            // 대상이 생명체(또는 데미지를 받을 수 있는 액터)라면 데미지 전달
            UGameplayStatics::ApplyDamage(Other, Damage, InstigatorController, this, UDamageType::StaticClass());
            
            // 벽 타격 이펙트는 부딪힌 표면의 각도(Normal)에 맞춰 재생
            Multicast_SpawnHitVFX(HitLocation, HitNormal.Rotation());
            
            // 이펙트 재생 시간을 벌기 위해 0.1초 지연 파괴
            SetLifeSpan(0.1f);
        }
    }
}

// ====================================================================================
// [Section 4] 네트워크 & 시각 효과 (RPC)
// ====================================================================================

void AMageProjectile::Multicast_SpawnHitVFX_Implementation(FVector Location, FRotator Rotation)
{
    // 1. 폭발(타격) 이펙트 스폰
    if (FireImpactVFX) 
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(),
            FireImpactVFX,
            Location,
            Rotation
        );
    }

    // 2. 가짜 죽음 처리 (Delete 되기 전 0.1초 동안 보이지 않도록 숨김)
    if (MeshComp) MeshComp->SetVisibility(false);
    if (NiagaraComp) NiagaraComp->SetVisibility(false);
    
    // 3. 연속 타격 방지를 위해 충돌 즉시 비활성화
    if (SphereComp) SphereComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 4. 벽을 뚫고 지나가지 않도록 즉시 이동 중지
    if (ProjectileMovement) ProjectileMovement->StopMovementImmediately();
}