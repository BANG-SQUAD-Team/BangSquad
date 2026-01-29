#include "SeaMonsterProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h" // 추가 필수!
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Character.h"
#include "Project_Bang_Squad/Character/PaladinCharacter.h"

ASeaMonsterProjectile::ASeaMonsterProjectile()
{
    PrimaryActorTick.bCanEverTick = false;

    // 1. 충돌체 생성 및 루트 설정
    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
    CollisionComp->InitSphereRadius(25.0f);
    RootComponent = CollisionComp;

    // 2. 콜리전 설정 (월드 다이나믹으로 변경)
    CollisionComp->SetCollisionProfileName(TEXT("Projectile")); // 기본 프로필 먼저 적용
    CollisionComp->SetCollisionObjectType(ECC_WorldDynamic);

    // 물리 충돌(OnHit)이 발생하도록 설정
    CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CollisionComp->SetNotifyRigidBodyCollision(true);

    CollisionComp->OnComponentHit.AddDynamic(this, &ASeaMonsterProjectile::OnHit);

    /** 3. 메쉬 설정 (충돌체에 부착) */
    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(CollisionComp);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 4. 이동 컴포넌트
    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileComp"));
    ProjectileMovement->UpdatedComponent = CollisionComp;
    ProjectileMovement->InitialSpeed = 0.0f;
    ProjectileMovement->MaxSpeed = 5000.0f;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->ProjectileGravityScale = 1.0f;
}

void ASeaMonsterProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    if (OtherActor && OtherActor != this)
    {
        // 1. 일단 팔라딘인지 확인
        APaladinCharacter* Paladin = Cast<APaladinCharacter>(OtherActor);

        if (Paladin)
        {
            // 2. [핵심] 부딪힌 컴포넌트(OtherComp)가 팔라딘의 '방패 메쉬'랑 똑같은 놈인지 확인
            if (OtherComp == Paladin->GetShieldMesh())
            {
                // 방패에 맞았으니 방패 체력 깎고 투사체는 즉시 소멸
                Paladin->ConsumeShield(10.0f);
                Destroy();
                return; // 여기서 끝! 아래 넉백 로직으로 안 내려감
            }
        }

        // 3. 방패가 아니라 몸에 맞았거나 다른 캐릭터인 경우
        if (ABaseCharacter* Victim = Cast<ABaseCharacter>(OtherActor))
        {
            // 이미 죽은 시체는 밀지 않음
            if (Victim->IsDead()) return;

            FVector LaunchDir = GetActorForwardVector();
            LaunchDir.Z = 0.5f;
            // 가차 없이 넉백
            Victim->LaunchCharacter(LaunchDir.GetSafeNormal() * KnockbackForce, true, true);
        }

        Destroy();
    }
}