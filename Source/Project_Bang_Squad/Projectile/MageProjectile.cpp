#include "Project_Bang_Squad/Projectile/MageProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

AMageProjectile::AMageProjectile()
{
    // 1. 구체 충돌체 설정 (이것만 충돌을 담당해야 함)
    SphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
    SphereComp->InitSphereRadius(15.0f);
    SphereComp->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    SphereComp->SetGenerateOverlapEvents(true); // 오버랩 이벤트 활성화
    RootComponent = SphereComp;

    // 2. 외형 메시 설정 (여기가 범인일 확률이 높습니다!)
    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(RootComponent);
    
    // [핵심] 메시의 콜리전을 완전히 꺼버립니다. (벽과 비비지 않게)
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);
    MeshComp->SetGenerateOverlapEvents(false);

    // 3. 발사체 이동 설정
    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->UpdatedComponent = SphereComp;
    ProjectileMovement->InitialSpeed = 3000.f;
    ProjectileMovement->MaxSpeed = 3000.f;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->ProjectileGravityScale = 0.f;

    SphereComp->OnComponentBeginOverlap.AddDynamic(this, &AMageProjectile::OnOverlap);
    
    InitialLifeSpan = 3.0f;
}

void AMageProjectile::BeginPlay()
{
    Super::BeginPlay();
}

void AMageProjectile::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
                                UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (OtherActor && OtherActor != GetOwner())
    {
        // 1. 즉시 물리 연산 중단 (이동 멈춤)
        if (ProjectileMovement)
        {
            ProjectileMovement->StopMovementImmediately();
        }

        // 2. 즉시 충돌 비활성화 및 시각적 제거
        SphereComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        MeshComp->SetVisibility(false);

        // 3. 데미지 전달
        UGameplayStatics::ApplyDamage(OtherActor, Damage, GetInstigatorController(), this, UDamageType::StaticClass());

        // 4. 파괴 (이제 지연 없이 사라집니다)
        Destroy();
    }
}