// Source/Project_Bang_Squad/Character/Enemy/Stage3MidBoss.cpp

#include "Project_Bang_Squad/Character/Enemy/Stage3MidBoss.h"
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Project_Bang_Squad/Character/Monsterbase/Stage3AIController.h"


AStage3MidBoss::AStage3MidBoss()
{
	PrimaryActorTick.bCanEverTick = true;
	AIControllerClass = AStage3AIController::StaticClass();
}

void AStage3MidBoss::BeginPlay()
{
	Super::BeginPlay();

	if (BossData)
	{
		if (UHealthComponent* HC = FindComponentByClass<UHealthComponent>())
		{
			HC->SetMaxHealth(BossData->MaxHealth);

			// [수정] SetHealth 대신 ApplyHeal을 사용하여 체력을 꽉 채워줍니다!
			HC->ApplyHeal(BossData->MaxHealth);
		}
	}
}

// ---------------------------------------------------------
// [AI 명령] 1. 근접 평타 실행
// ---------------------------------------------------------
float AStage3MidBoss::Execute_BasicAttack()
{
	// 공용 변수인 AttackMontages의 첫 번째 슬롯 사용
	if (BossData && BossData->AttackMontages.Num() > 0)
	{
		UAnimMontage* MontageToPlay = BossData->AttackMontages[0];
		if (MontageToPlay)
		{
			Multicast_PlayMontage(MontageToPlay);
			return MontageToPlay->GetPlayLength();
		}
	}
	return 0.0f;
}


// ---------------------------------------------------------
// [AI 명령] 2. 원거리 공격 실행
// ---------------------------------------------------------
float AStage3MidBoss::Execute_RangedAttack(AActor* Target)
{
	if (!Target || !BossData || !BossData->Stage3RangedAttackMontage) return 0.0f;

	// [핵심 1] AI가 넘겨준 타겟을 변수에 기억해 둡니다!
	CurrentTarget = Target;

	// 타겟을 향해 몸을 돌림
	FVector Direction = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	Direction.Z = 0;
	SetActorRotation(Direction.Rotation());

	// 몽타주 재생
	Multicast_PlayMontage(BossData->Stage3RangedAttackMontage);
	return BossData->Stage3RangedAttackMontage->GetPlayLength();
}

// ---------------------------------------------------------
// [노티파이] 근접 평타 데미지 판정 (서버만)
// ---------------------------------------------------------
void AStage3MidBoss::OnNotify_MeleeHitCheck()
{
	if (!HasAuthority()) return;

	FVector StartLoc = GetActorLocation() + (GetActorForwardVector() * 150.0f);
	float Radius = 100.0f;
	float Damage = BossData ? BossData->AttackDamage : 20.0f;

	TArray<FOverlapResult> OverlapResults;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(Radius);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	if (GetWorld()->OverlapMultiByChannel(OverlapResults, StartLoc, FQuat::Identity, ECC_Pawn, Sphere, Params))
	{
		for (const FOverlapResult& Overlap : OverlapResults)
		{
			if (ABaseCharacter* HitPlayer = Cast<ABaseCharacter>(Overlap.GetActor()))
			{
				if (!HitPlayer->IsDead())
				{
					UGameplayStatics::ApplyDamage(HitPlayer, Damage, GetController(), this, UDamageType::StaticClass());
				}
			}
		}
	}
}

// ---------------------------------------------------------
// [노티파이] 원거리 투사체 발사 시점 (서버만)
// ---------------------------------------------------------
void AStage3MidBoss::OnNotify_FireRanged()
{
	if (!HasAuthority()) return;

	// [핵심 2] 블랙보드를 뒤질 필요 없이, 아까 기억해둔 타겟에게 바로 쏩니다!
	FireProjectile(CurrentTarget);
}

void AStage3MidBoss::FireProjectile(AActor* Target)
{
	if (!BossData || !BossData->Stage3RangedProjectileClass) return;

	float BossRadius = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() : 50.0f;
	FVector SpawnLoc = GetActorLocation() + (GetActorForwardVector() * (BossRadius + 50.0f));
	SpawnLoc.Z += 50.0f; // 가슴 높이에서 발사

	// 타겟이 있으면 타겟 방향으로, 없으면 그냥 앞을 향해 쏜다
	FRotator SpawnRot = GetActorRotation();
	if (Target)
	{
		FVector DirToTarget = (Target->GetActorLocation() - SpawnLoc).GetSafeNormal();
		SpawnRot = DirToTarget.Rotation();
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* Projectile = GetWorld()->SpawnActor<AActor>(BossData->Stage3RangedProjectileClass, SpawnLoc, SpawnRot, Params);

	// 투사체가 나 자신(보스)을 때리지 않도록 예외 처리
	if (Projectile)
	{
		if (UPrimitiveComponent* Primitive = Projectile->FindComponentByClass<UPrimitiveComponent>())
		{
			Primitive->IgnoreActorWhenMoving(this, true);
		}

		if (UProjectileMovementComponent* PMC = Projectile->FindComponentByClass<UProjectileMovementComponent>())
		{
			float Speed = (PMC->InitialSpeed > 0) ? PMC->InitialSpeed : 1500.0f;
			PMC->SetVelocityInLocalSpace(FVector(Speed, 0, 0));
		}
	}
}

// ---------------------------------------------------------
// [멀티캐스트] 몽타주 동기화
// ---------------------------------------------------------
void AStage3MidBoss::Multicast_PlayMontage_Implementation(UAnimMontage* Montage)
{
	if (Montage) PlayAnimMontage(Montage);
}