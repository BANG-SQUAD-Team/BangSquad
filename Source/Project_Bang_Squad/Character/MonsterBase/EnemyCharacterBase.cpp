#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h"

#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"

AEnemyCharacterBase::AEnemyCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;

	bUseControllerRotationYaw = false;

	// 멀티 기준: 최소한 액터는 복제
	bReplicates = true;
}

void AEnemyCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		DefaultMaxWalkSpeed = Move->MaxWalkSpeed;

		Move->bUseControllerDesiredRotation = true;
		Move->RotationRate = FRotator(0.f, 720.f, 0.f);
	}
}

// =========================
// Hit React
// =========================

void AEnemyCharacterBase::ReceiveHitReact()
{
	if (!HasAuthority()) return;
	if (bIsDead) return;

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

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		// BeginPlay 전에 호출될 수도 있으니 보험
		if (DefaultMaxWalkSpeed <= 0.f)
		{
			DefaultMaxWalkSpeed = Move->MaxWalkSpeed;
		}

		Move->MaxWalkSpeed = DefaultMaxWalkSpeed * HitReactSpeedMultiplier;
	}

	Multicast_PlayHitReactMontage();

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
	if (bIsDead) return;

	PlayAnimMontage(HitReactMontage);
}

// =========================
// Death
// =========================

void AEnemyCharacterBase::ReceiveDeath()
{
	if (!HasAuthority()) return;
	if (bIsDead) return;

	StartDeath();
}

void AEnemyCharacterBase::StartDeath()
{
	bIsDead = true;
	bIsHitReacting = false;

	// AI 이동/추적 완전 차단
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->StopMovement();
		AIC->ClearFocus(EAIFocusPriority::Gameplay);
	}

	// 캐릭터 이동 차단
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}

	// 캡슐 충돌은 상황에 따라 유지/비활성화 선택
	// (시체가 길막하면 꺼버리는 게 편함)
	// GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 데스 애니 재생(전 클라)
	Multicast_PlayDeathMontage();

	// 래그돌 전환 예약
	GetWorldTimerManager().ClearTimer(DeathToRagdollTimer);
	if (bEnableRagdollOnDeath)
	{
		GetWorldTimerManager().SetTimer(
			DeathToRagdollTimer,
			this,
			&AEnemyCharacterBase::EnterRagdoll,
			DeathToRagdollDelay,
			false
		);
	}

	// 파괴 예약
	if (bDestroyAfterDeath)
	{
		SetLifeSpan(DestroyDelay);
	}
}

void AEnemyCharacterBase::Multicast_PlayDeathMontage_Implementation()
{
	if (!DeathMontage) return;

	PlayAnimMontage(DeathMontage);

	// 루프를 원하면, 몽타주에 "Loop" 섹션을 만들고 여기서 루프를 걸어라.
	// (섹션이 없으면 아무 일도 안 함)
	if (bLoopDeathMontage && DeathLoopSectionName != NAME_None)
	{
		if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			// Loop 섹션이 존재한다면 자기 자신으로 next section을 걸어 반복
			AnimInst->Montage_SetNextSection(DeathLoopSectionName, DeathLoopSectionName, DeathMontage);
		}
	}
}

void AEnemyCharacterBase::EnterRagdoll()
{
	// 서버에서 한 번 더 멀티캐스트로 전환(클라별로 시체 물리는 조금 달라도 OK)
	if (!HasAuthority()) return;

	Multicast_EnterRagdoll();
}

void AEnemyCharacterBase::Multicast_EnterRagdoll_Implementation()
{
	if (!GetMesh()) return;

	// 몽타주가 계속 루프 돌고 있으면 래그돌이 먹기 어렵다.
	// 래그돌로 갈 때는 애니를 끊는 게 보통 더 자연스럽다.
	if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
	{
		AnimInst->StopAllMontages(0.1f);
	}

	// 캡슐은 보통 꺼서 시체가 이상하게 튕기는 걸 방지
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 메시는 래그돌 프로파일(프로젝트에 없으면 PhysicsActor로 바꿔도 됨)
	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->WakeAllRigidBodies();
	GetMesh()->bBlendPhysics = true;
}
