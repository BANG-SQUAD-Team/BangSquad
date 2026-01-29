#include "SeaMonsterProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h" // 추가 필수!
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Character.h"
#include "Project_Bang_Squad/Character/PaladinCharacter.h"

ASeaMonsterProjectile::ASeaMonsterProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	// 1. 충돌체 설정 (Root)
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	CollisionComp->InitSphereRadius(25.0f);
	CollisionComp->BodyInstance.SetCollisionProfileName("Projectile");
	CollisionComp->OnComponentHit.AddDynamic(this, &ASeaMonsterProjectile::OnHit);
	RootComponent = CollisionComp;

	/** 2. 메쉬 설정 (충돌체에 부착) */
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(CollisionComp); // 충돌체를 따라다니게 설정
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 충돌은 구체(CollisionComp)가 담당하니 메쉬는 끔

	// 3. 이동 컴포넌트
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
		// 1. 먼저 팔라딘인지 확인합니다.
		APaladinCharacter* Paladin = Cast<APaladinCharacter>(OtherActor);
		if (Paladin)
		{
			// 투사체가 날아가는 방향을 계산합니다.
			FVector ProjectileDir = GetVelocity().GetSafeNormal();

			// 2. 팔라딘이 이 방향의 공격을 방패로 막고 있는지 검사합니다.
			if (Paladin->IsBlockingDirection(ProjectileDir))
			{
				// [방어 성공] 방패 체력을 일정량 깎고 투사체만 없앱니다.
				// 넉백(LaunchCharacter)을 호출하지 않고 return 하므로 무효화됩니다.
				Paladin->ConsumeShield(10.0f); // 원하는 데미지 양 설정

				// 방패에 막히는 이펙트나 사운드를 여기서 재생하면 더 좋습니다.
				Destroy();
				return;
			}
		}

		// 3. 방어에 실패했거나 팔라딘이 아닌 일반 캐릭터일 경우 (기존 넉백 로직)
		if (ACharacter* Victim = Cast<ACharacter>(OtherActor))
		{
			FVector LaunchDir = GetActorForwardVector();
			LaunchDir.Z = 0.5f;
			Victim->LaunchCharacter(LaunchDir.GetSafeNormal() * KnockbackForce, true, true);
		}

		Destroy();
	}
}