#include "LichProjectile.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

ALichProjectile::ALichProjectile()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    // 1. 충돌체 (Sphere)
    SphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
    SphereComp->InitSphereRadius(20.0f);
    SphereComp->SetCollisionProfileName(TEXT("Projectile"));

    // [충돌 설정]
    SphereComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SphereComp->SetCollisionResponseToAllChannels(ECR_Ignore);
    SphereComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);  // 벽
    SphereComp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block); // 문, 바닥
    SphereComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);       // 플레이어 (관통X, 감지O)

    RootComponent = SphereComp;

    // 2. 이펙트 (Niagara)
    NiagaraComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraComp"));
    NiagaraComp->SetupAttachment(RootComponent);

    // 3. 움직임 (ProjectileMovement)
    MovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("MovementComp"));
    MovementComp->UpdatedComponent = SphereComp;
    MovementComp->InitialSpeed = 1500.0f; // 기본 속도 (BP에서 수정 가능)
    MovementComp->MaxSpeed = 1500.0f;
    MovementComp->bRotationFollowsVelocity = true;
    MovementComp->ProjectileGravityScale = 0.0f; // 직선으로 날아가도록

    // 이벤트 연결
    SphereComp->OnComponentBeginOverlap.AddDynamic(this, &ALichProjectile::OnOverlap);

    // 안전장치: 5초 뒤 자동 삭제
    InitialLifeSpan = 5.0f;
}

void ALichProjectile::BeginPlay()
{
    Super::BeginPlay();

    // 발사한 본인(리치 보스)과는 충돌하지 않도록 예외 처리
    if (GetOwner())
    {
        SphereComp->IgnoreActorWhenMoving(GetOwner(), true);
    }
}

void ALichProjectile::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // 서버에서만 데미지 처리
    if (!HasAuthority()) return;
    if (!OtherActor || OtherActor == GetOwner()) return;

    // [핵심] 플레이어 태그가 있거나 Pawn이면 데미지 적용
    if (OtherActor->ActorHasTag(TEXT("Player")) || OtherActor->IsA(APawn::StaticClass()))
    {
        // 1. 데미지
        UGameplayStatics::ApplyDamage(
            OtherActor,
            Damage,
            GetInstigatorController(),
            this,
            UDamageType::StaticClass()
        );

        // 2. 이펙트 (폭발)
        if (HitVFX)
        {
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), HitVFX, GetActorLocation());
        }

        // 3. 삭제 (관통하지 않고 사라짐)
        Destroy();
    }
}

void ALichProjectile::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
    Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

    if (!HasAuthority()) return;

    // 벽에 닿아도 이펙트 터뜨리고 삭제
    if (HitVFX)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), HitVFX, HitLocation);
    }
    Destroy();
}