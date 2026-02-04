#include "SeaMonsterProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
// �ʿ��� ������� ���⿡ ��� ����
#include "Kismet/GameplayStatics.h"
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
	if (OtherActor && OtherActor != this)
	{
		// 1. 팔라딘인지 확인
		APaladinCharacter* Paladin = Cast<APaladinCharacter>(OtherActor);

		if (Paladin)
		{
			// 내 진행 방향(Forward)이 팔라딘의 방어 각도인지 확인
			// 투사체가 날아가는 방향 = 공격이 들어오는 방향
			if (Paladin->IsBlockingDirection(GetActorForwardVector()))
			{
				// A. 방어 성공!
				// 1. 방패 체력 깎기 (기존 함수 활용)
				Paladin->ConsumeShield(10.0f); 
                
				// 2. 투사체 파괴
				Destroy();
                
				// 3. 넉백 및 데미지 로직 실행하지 않고 종료! (여기가 핵심)
				return; 
			}
		}

		// 2. 방어 실패 또는 일반 캐릭터 피격 로직
		if (ABaseCharacter* Victim = Cast<ABaseCharacter>(OtherActor))
		{
			if (Victim->IsDead()) return;

			// 넉백 (Launch)
			FVector LaunchDir = GetActorForwardVector();
			LaunchDir.Z = 0.5f;
			Victim->LaunchCharacter(LaunchDir.GetSafeNormal() * KnockbackForce, true, true);
            
			// 데미지 적용
			// 여기서 ApplyDamage를 호출하면 Paladin의 TakeDamage가 호출되지만,
			// 이미 위에서 '방어 성공'을 처리했으므로 여기는 '방어 실패' 상황입니다.
			UGameplayStatics::ApplyDamage(Victim, 10.0f, GetInstigatorController(), this, UDamageType::StaticClass());
		}

		Destroy();
	}
}