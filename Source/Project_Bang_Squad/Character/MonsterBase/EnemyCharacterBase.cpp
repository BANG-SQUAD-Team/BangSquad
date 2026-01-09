#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h"

#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

//  여기서만 포함 (헤더에서는 전방선언만)
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"

AEnemyCharacterBase::AEnemyCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bUseControllerRotationYaw = false;

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

	// =========================
	// HealthComponent 자동 연동
	// =========================
	if (UHealthComponent* HC = FindComponentByClass<UHealthComponent>())
	{
		HC->OnDead.RemoveDynamic(this, &AEnemyCharacterBase::HandleDeadFromHealth);
		HC->OnDead.AddDynamic(this, &AEnemyCharacterBase::HandleDeadFromHealth);

		HC->OnHealthChanged.RemoveDynamic(this, &AEnemyCharacterBase::HandleHealthChangedFromHealth);
		HC->OnHealthChanged.AddDynamic(this, &AEnemyCharacterBase::HandleHealthChangedFromHealth);
	}
}

void AEnemyCharacterBase::OnDeathStarted()
{
}

void AEnemyCharacterBase::HandleDeadFromHealth()
{
	ReceiveDeath();
}

void AEnemyCharacterBase::HandleHealthChangedFromHealth(float NewHealth, float InMaxHealth)
{
	if (!HasAuthority()) return;
	if (bIsDead) return;
	if (NewHealth <= 0.f) return;

	ReceiveHitReact();
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

	OnDeathStarted();

	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->StopMovement();
		AIC->ClearFocus(EAIFocusPriority::Gameplay);
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}

	Multicast_PlayDeathMontage();

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

	if (bDestroyAfterDeath)
	{
		SetLifeSpan(DestroyDelay);
	}
}

void AEnemyCharacterBase::Multicast_PlayDeathMontage_Implementation()
{
	if (!DeathMontage) return;

	PlayAnimMontage(DeathMontage);

	if (bLoopDeathMontage && DeathLoopSectionName != NAME_None)
	{
		if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			AnimInst->Montage_SetNextSection(DeathLoopSectionName, DeathLoopSectionName, DeathMontage);
		}
	}
}

void AEnemyCharacterBase::EnterRagdoll()
{
	if (!HasAuthority()) return;
	Multicast_EnterRagdoll();
}

void AEnemyCharacterBase::Multicast_EnterRagdoll_Implementation()
{
	if (!GetMesh()) return;

	if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
	{
		AnimInst->StopAllMontages(0.1f);
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->WakeAllRigidBodies();
	GetMesh()->bBlendPhysics = true;
}
