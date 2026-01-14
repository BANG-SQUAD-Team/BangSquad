#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimMontage.h"
#include "TimerManager.h"

AEnemyCharacterBase::AEnemyCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false; // 기본 OFF

	// 바라보기 허용: AIController가 SetFocus로 회전하는 패턴을 쓰기 좋게
	bUseControllerRotationYaw = false;
}

void AEnemyCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		DefaultMaxWalkSpeed = Move->MaxWalkSpeed;

		// AI가 원하는 방향으로 회전(바라보기) 가능하게
		Move->bUseControllerDesiredRotation = true;
		Move->RotationRate = FRotator(0.f, 720.f, 0.f);
	}
}

void AEnemyCharacterBase::ReceiveHitReact()
{
	// 멀티 기준: 서버에서 시작하는 게 정석
	if (!HasAuthority())
	{
		return;
	}

	if (bIsHitReacting && bIgnoreHitReactWhileActive)
	{
		return;
	}

	float Duration = HitReactMinDuration;
	if (HitReactMontage)
	{
		Duration = FMath::Max(Duration, HitReactMontage->GetPlayLength());
	}

	StartHitReact(Duration);
}

void AEnemyCharacterBase::StartHitReact(float Duration)
{
	bIsHitReacting = true;

	// 속도만 감소
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = DefaultMaxWalkSpeed * HitReactSpeedMultiplier;
	}

	// 전 클라에 몽타주 재생
	Multicast_PlayHitReactMontage();

	// Duration 후 복구
	GetWorldTimerManager().ClearTimer(HitReactTimer);
	GetWorldTimerManager().SetTimer(HitReactTimer, this, &AEnemyCharacterBase::EndHitReact, Duration, false);
}

void AEnemyCharacterBase::EndHitReact()
{
	bIsHitReacting = false;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = DefaultMaxWalkSpeed;
	}
}

void AEnemyCharacterBase::Multicast_PlayHitReactMontage_Implementation()
{
	if (!HitReactMontage) return;

	PlayAnimMontage(HitReactMontage);
}
