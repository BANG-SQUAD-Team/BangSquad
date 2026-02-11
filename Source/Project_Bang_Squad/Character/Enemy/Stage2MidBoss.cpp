// Source/Project_Bang_Squad/Character/Enemy/Stage2MidBoss.cpp

#include "Project_Bang_Squad/Character/Enemy/Stage2MidBoss.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h" // 모듈 추가 필요 (NavigationSystem)

AStage2MidBoss::AStage2MidBoss()
{
	bReplicates = true;
	SetReplicateMovement(true);
}

void AStage2MidBoss::BeginPlay()
{
	Super::BeginPlay();
}

float AStage2MidBoss::Execute_RangedAttack(AActor* Target)
{
	if (!HasAuthority() || !BossData) return 0.0f;

	// 회전 등 준비 동작은 유지
	if (Target)
	{
		FVector Dir = Target->GetActorLocation() - GetActorLocation();
		Dir.Z = 0.0f;
		SetActorRotation(Dir.Rotation());
	}

	// 투사체 발사는 여기서 안 함! (노티파이로 이관)

	// 애니메이션 재생
	if (BossData->MagicAttackMontage)
	{
		Multicast_PlayMontage(BossData->MagicAttackMontage);
		return BossData->MagicAttackMontage->GetPlayLength();
	}
	return 1.0f;
}

// 2. [추가] 마법 발사 노티파이 (실제 발사)
void AStage2MidBoss::OnNotify_FireMagic()
{
	if (!HasAuthority() || !BossData) return;

	// 여기서 투사체 생성!
	if (BossData->MagicProjectileClass)
	{
		FVector SpawnLoc = GetActorLocation() + GetActorForwardVector() * 100.0f;

		// 소켓이 있다면 소켓 위치 사용 권장
		if (GetMesh()->DoesSocketExist(TEXT("Muzzle_01")))
			SpawnLoc = GetMesh()->GetSocketLocation(TEXT("Muzzle_01"));

		FRotator SpawnRot = GetActorRotation();

		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.Instigator = this;

		GetWorld()->SpawnActor<AActor>(BossData->MagicProjectileClass, SpawnLoc, SpawnRot, Params);
	}
}



bool AStage2MidBoss::Execute_TeleportToTarget(AActor* Target)
{
	if (!HasAuthority() || !Target || !BossData) return false;

	// 1. 사라지는 이펙트
	Multicast_TeleportFX(GetActorLocation());

	// 2. 목표 위치 계산 (타겟 뒤쪽 150 거리)
	FVector TargetLoc = Target->GetActorLocation();
	FVector TargetBack = -Target->GetActorForwardVector();
	FVector DestLoc = TargetLoc + (TargetBack * 150.0f);

	// 3. NavMesh 투영 (벽/바닥 뚫림 방지)
	FNavLocation NavLoc;
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (NavSys && NavSys->ProjectPointToNavigation(DestLoc, NavLoc, FVector(200.f, 200.f, 200.f)))
	{
		DestLoc = NavLoc.Location;
		DestLoc.Z += GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 10.0f; // 높이 보정
	}

	// 4. 이동 및 회전
	TeleportTo(DestLoc, (TargetLoc - DestLoc).Rotation());

	// 5. 나타나는 이펙트 & 애니메이션
	Multicast_TeleportFX(DestLoc);
	if (BossData->TeleportMontage)
	{
		Multicast_PlayMontage(BossData->TeleportMontage);
		return true;
	}
	return true;
}

float AStage2MidBoss::Execute_MeleeAttack()
{
	if (!HasAuthority() || !BossData) return 0.0f;

	if (BossData->MeleeAttackMontage)
	{
		Multicast_PlayMontage(BossData->MeleeAttackMontage);
		return BossData->MeleeAttackMontage->GetPlayLength();
	}
	return 1.0f;
}

// 4. [추가] 근접 타격 노티파이 (실제 판정)
void AStage2MidBoss::OnNotify_MeleeHitCheck()
{
	if (!HasAuthority() || !BossData) return;

	// Sphere Trace 로직을 여기로 이동
	FVector Start = GetActorLocation();
	FVector End = Start + (GetActorForwardVector() * BossData->MeleeAttackRadius);

	TArray<FHitResult> HitResults;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	bool bHit = GetWorld()->SweepMultiByChannel(
		HitResults, Start, End, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeSphere(BossData->MeleeAttackRadius), Params
	);

	if (bHit)
	{
		for (const FHitResult& Hit : HitResults)
		{
			if (AActor* HitActor = Hit.GetActor())
			{
				if (HitActor != this && IsValid(HitActor))
				{
					UGameplayStatics::ApplyDamage(
						HitActor, BossData->MeleeAttackDamage, GetController(), this, UDamageType::StaticClass()
					);
				}
			}
		}
	}
}

void AStage2MidBoss::Multicast_PlayMontage_Implementation(UAnimMontage* Montage)
{
	if (Montage) PlayAnimMontage(Montage);
}

void AStage2MidBoss::Multicast_TeleportFX_Implementation(FVector Location)
{
	if (BossData && BossData->TeleportVFX)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BossData->TeleportVFX, Location);
	}
}