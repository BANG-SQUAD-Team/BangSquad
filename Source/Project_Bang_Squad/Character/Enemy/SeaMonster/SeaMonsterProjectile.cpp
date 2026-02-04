#include "SeaMonsterProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
// 필요한 헤더들을 여기에 모두 포함
#include "Project_Bang_Squad/Character/PaladinCharacter.h" 

ASeaMonsterProjectile::ASeaMonsterProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	CollisionComp->InitSphereRadius(25.0f);
	RootComponent = CollisionComp;

	CollisionComp->SetCollisionProfileName(TEXT("Projectile"));
	CollisionComp->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionComp->SetNotifyRigidBodyCollision(true);
	CollisionComp->OnComponentHit.AddDynamic(this, &ASeaMonsterProjectile::OnHit);

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(CollisionComp);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileComp"));
	ProjectileMovement->UpdatedComponent = CollisionComp;
	ProjectileMovement->InitialSpeed = 0.0f;
	ProjectileMovement->MaxSpeed = 5000.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->ProjectileGravityScale = 1.0f;
}

void ASeaMonsterProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!OtherActor || OtherActor == this) return;

	UE_LOG(LogTemp, Warning, TEXT("[OnHit] Hit Actor: %s, Component: %s"), *OtherActor->GetName(), *OtherComp->GetName());

	// 1. 팔라딘인지 확인
	APaladinCharacter* Paladin = Cast<APaladinCharacter>(OtherActor);
	if (Paladin)
	{
		// 2. 방패에 맞았는지 확인
		if (OtherComp == Paladin->GetShieldMesh())
		{
			UE_LOG(LogTemp, Warning, TEXT("[OnHit] >> Shield Hit Success!"));
			Paladin->ConsumeShield(10.0f);
			Destroy();
			return;
		}
	}

	// 3. 일반 캐릭터 피격 처리 (넉백)
	if (ABaseCharacter* Victim = Cast<ABaseCharacter>(OtherActor))
	{
		if (!Victim->IsDead())
		{
			UE_LOG(LogTemp, Warning, TEXT("[OnHit] >> Knockback Applied to %s"), *Victim->GetName());
			FVector LaunchDir = GetActorForwardVector();
			LaunchDir.Z = 0.5f;
			Victim->LaunchCharacter(LaunchDir.GetSafeNormal() * KnockbackForce, true, true);
		}
	}

	Destroy();
}