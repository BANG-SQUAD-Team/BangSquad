#include "Project_Bang_Squad/Character/Player/Titan/TitanThrowableActor.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h"

ATitanThrowableActor::ATitanThrowableActor()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;

	MeshComp->SetMobility(EComponentMobility::Movable);
	MeshComp->SetSimulatePhysics(true);
	MeshComp->SetEnableGravity(true);

	MeshComp->SetCollisionProfileName(TEXT("PhysicsActor"));
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	// 물리 충돌 이벤트를 켜줍니다.
	MeshComp->SetNotifyRigidBodyCollision(true);
}

void ATitanThrowableActor::BeginPlay()
{
	Super::BeginPlay();

	// 충돌 함수 바인딩 (서버에서만 발생하도록 보통 설정하지만, 여기서는 액터 자체에 바인딩합니다)
	if (HasAuthority())
	{
		MeshComp->OnComponentHit.AddDynamic(this, &ATitanThrowableActor::OnHit);
	}
}

void ATitanThrowableActor::OnThrown_Implementation(FVector Direction)
{
	// 던져졌다는 상태를 켭니다.
	bIsThrown = true;

	// 던지는 순간의 물리적 힘을 가할 수도 있습니다.
	// MeshComp->AddImpulse(Direction * 3000.f, NAME_None, true);
}

void ATitanThrowableActor::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 던져진 상태이고, 아직 터지지 않았다면 폭발!
	if (bIsThrown && !bHasExploded)
	{
		Explode();
	}
}

void ATitanThrowableActor::Explode()
{
	bHasExploded = true;

	FVector ExplodeLocation = GetActorLocation();

	// 데미지와 넉백은 반드시 서버에서만 처리합니다.
	if (HasAuthority())
	{
		TArray<FHitResult> HitResults;
		FCollisionShape Sphere = FCollisionShape::MakeSphere(ExplosionRadius);

		// 반경 내의 액터들을 찾습니다.
		bool bHit = GetWorld()->SweepMultiByChannel(HitResults, ExplodeLocation, ExplodeLocation, FQuat::Identity, ECC_Pawn, Sphere);

		if (bHit)
		{
			// 여러 부위에 맞아 중복 처리되는 것을 방지하기 위한 배열
			TSet<AActor*> ProcessedActors;

			for (const FHitResult& Hit : HitResults)
			{
				AActor* HitActor = Hit.GetActor();

				// 자신(투척물)이 아니고, 아직 처리하지 않은 액터라면
				if (HitActor && HitActor != this && !ProcessedActors.Contains(HitActor))
				{
					ProcessedActors.Add(HitActor);

					// =========================================================
					// 1. 데미지 처리: 에너미(AEnemyCharacterBase)에게만 줍니다.
					// =========================================================
					if (HitActor->IsA(AEnemyCharacterBase::StaticClass()))
					{
						UGameplayStatics::ApplyDamage(
							HitActor,
							ExplosionDamage,
							GetInstigatorController(),
							this,
							UDamageType::StaticClass()
						);
					}

					// =========================================================
					// 2. 넉백 처리: 에너미든 플레이어든 맞은 대상이 캐릭터면 모두 날립니다.
					// =========================================================
					if (ACharacter* HitCharacter = Cast<ACharacter>(HitActor))
					{
						FVector LaunchDir = HitActor->GetActorLocation() - ExplodeLocation;

						// 캐릭터가 위로 확 뜰 수 있게 Z축 방향 힘을 살짝 더해줍니다.
						LaunchDir.Z += 200.f;
						LaunchDir.Normalize();

						// LaunchCharacter로 물리적으로 날려버립니다.
						HitCharacter->LaunchCharacter(LaunchDir * KnockbackForce, true, true);
					}
					// 만약 캐릭터가 아닌 일반 물리 박스/드럼통 같은 오브젝트라면
					else if (UPrimitiveComponent* HitPrimComp = Cast<UPrimitiveComponent>(HitActor->GetRootComponent()))
					{
						if (HitPrimComp->IsSimulatingPhysics())
						{
							HitPrimComp->AddRadialImpulse(ExplodeLocation, ExplosionRadius, KnockbackForce, ERadialImpulseFalloff::RIF_Linear, true);
						}
					}
				}
			}
		}

		// 시각적 효과(이펙트/사운드)를 모든 클라이언트에 재생하라고 명령합니다.
		Multicast_PlayExplosionEffects();

		// 처리가 끝났으니 서버에서 액터를 파괴합니다.
		Destroy();
	}
}

void ATitanThrowableActor::Multicast_PlayExplosionEffects_Implementation()
{

}