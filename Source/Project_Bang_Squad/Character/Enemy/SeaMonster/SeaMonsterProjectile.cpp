#include "SeaMonsterProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h" // 추가 필수!
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Character.h"

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
		if (ACharacter* Victim = Cast<ACharacter>(OtherActor))
		{
			FVector LaunchDir = GetActorForwardVector();
			LaunchDir.Z = 0.5f;
			// 메이지를 물로 냅다 던져버림
			Victim->LaunchCharacter(LaunchDir.GetSafeNormal() * KnockbackForce, true, true);
		}
		Destroy();
	}
}