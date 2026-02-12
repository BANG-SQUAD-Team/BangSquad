// Source/Project_Bang_Squad/Character/StageBoss/Stage1Boss.cpp

#include "Stage1Boss.h"
#include "JobCrystal.h"
#include "DeathWall.h"
#include "BossSpikeTrap.h"
#include "AQTEObject.h" 
#include "StageBossGameMode.h"
#include "StageBossGameState.h"
#include "Project_Bang_Squad/BossPattern/Boss1_Rampart.h"
#include "Project_Bang_Squad/Projectile/SlashProjectile.h"
#include "Project_Bang_Squad/Core/TrueDamageType.h"
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h"

#include "Animation/AnimMontage.h"
#include "Components/BoxComponent.h"
#include "Engine/DamageEvents.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Net/UnrealNetwork.h"
#include "Project_Bang_Squad/Game/Stage/MapPortal.h"

AStage1Boss::AStage1Boss()
{
	bReplicates = true;
}

void AStage1Boss::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AStage1Boss, bHasTriggeredQTE_10);
}

void AStage1Boss::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		// [����] ���� ���(100) ũ����Ż ��� ����
		// �������ڸ��� SetPhase(Gimmick)�� ȣ���Ͽ� ���� ���·� ����� ������ ��ȯ�մϴ�.
		if (!bHasTriggeredCrystal_100)
		{
			bHasTriggeredCrystal_100 = true;
			SetPhase(EBossPhase::Gimmick); // ���������� SpawnCrystals() ȣ���
			UE_LOG(LogTemp, Warning, TEXT("[BOSS] Encounter: 100%% Crystal Gimmick Started"));
		}

		if (UHealthComponent* HC = FindComponentByClass<UHealthComponent>())
		{
			HC->OnHealthChanged.AddDynamic(this, &AStage1Boss::OnHealthChanged);
		}

		if (!IsValid(MeleeCollisionBox))
		{
			MeleeCollisionBox = Cast<UBoxComponent>(GetComponentByClass(UBoxComponent::StaticClass()));
		}
	}
}

// ==============================================================================
// [1] ������ ó�� �� ���(100, 50, 10) �ߵ� ����
// ==============================================================================

// Source/Project_Bang_Squad/Character/StageBoss/Stage1Boss.cpp

// ==============================================================================
// [1] ������ ó�� �� ���(100, 50, 0) �ߵ� ���� (�ٽ�)
// ==============================================================================

// Source/Project_Bang_Squad/Character/StageBoss/Stage1Boss.cpp

float AStage1Boss::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	
	bool bIsTrueDamage = (DamageEvent.DamageTypeClass == UTrueDamageType::StaticClass());

	
	if (!bIsTrueDamage)
	{
		
		if (!HasAuthority() || bHasTriggeredQTE_10 || bIsInvincible) return 0.0f;

		
		AStageBossGameState* GS = GetWorld()->GetGameState<AStageBossGameState>();
		if (GS && GS->bIsQTEActive) return 0.0f;

		
		AActor* Attacker = DamageCauser;
		if (EventInstigator && EventInstigator->GetPawn()) Attacker = EventInstigator->GetPawn();
		if (Attacker && (Attacker == this || Attacker->IsA(AEnemyCharacterBase::StaticClass()))) return 0.0f;
	}

	
	float ActualDamage = DamageAmount;

	if (UHealthComponent* HC = FindComponentByClass<UHealthComponent>())
	{
		float CurrentHP = HC->GetHealth();
		float MaxHP = HC->MaxHealth;

		
		if (!bHasTriggeredCrystal_50 && (CurrentHP - DamageAmount) / MaxHP <= 0.5f)
		{
			if (bIsDeathWallSequenceActive) FinishDeathWallPattern();

			if (CurrentPhase == EBossPhase::Phase1)
			{
				bHasTriggeredCrystal_50 = true;
				Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
				SetPhase(EBossPhase::Gimmick);
				UE_LOG(LogTemp, Warning, TEXT("[BOSS] 50%% HP Reached: Starting Crystal Gimmick!"));
				return ActualDamage;
			}
		}

		
		if (!bIsTrueDamage && (CurrentHP - DamageAmount) <= 1.0f && !bHasTriggeredQTE_10)
		{
			
			ActualDamage = FMath::Max(0.0f, CurrentHP - 1.0f);

			
			Super::TakeDamage(ActualDamage, DamageEvent, EventInstigator, DamageCauser);

			// QTE
			bHasTriggeredQTE_10 = true;
			if (AStageBossGameState* GS = GetWorld()->GetGameState<AStageBossGameState>())
				GS->SetQTEActive(true);

			bIsInvincible = true;

			if (AAIController* AIC = Cast<AAIController>(GetController())) AIC->StopMovement();

			// GameMode-QTE 
			if (AStageBossGameMode* GM = GetWorld()->GetAuthGameMode<AStageBossGameMode>())
			{
				GM->TriggerSpearQTE(this);
				UE_LOG(LogTemp, Warning, TEXT("[BOSS] HP 1 Reached! Finale QTE Triggered!"));
			}

			//
			return ActualDamage;
		}
	}
		
	return Super::TakeDamage(ActualDamage, DamageEvent, EventInstigator, DamageCauser);
}

// GameMode ȣ��: ���־� ���� (���� -> ��Ƽĳ��Ʈ)
void AStage1Boss::PlayQTEVisuals(float Duration)
{
	if (!HasAuthority()) return;

	// 1. ���� ���� ��Ÿ�� ��� (���ļ� �涱�� ��)
	if (BossData && BossData->QTE_TelegraphMontage)
	{
		// ��Ÿ�� ���
		Multicast_PlayAttackMontage(BossData->QTE_TelegraphMontage);

		// ��Ÿ�� ���ڶ����� ����(Freeze) ��Ű�� ���� Ÿ�̸�
		// (��Ÿ�� ���̿� ���� ������ ���� �ʿ�, ���⼱ ���÷� 90% ����)
		float MontageLength = BossData->QTE_TelegraphMontage->GetPlayLength();
		FTimerHandle FreezeTimer;
		GetWorldTimerManager().SetTimer(FreezeTimer, [this]()
			{
				Multicast_FreezeAnimation(true);
			}, MontageLength - 0.1f, false);

		UE_LOG(LogTemp, Log, TEXT("[BOSS] Playing QTE Telegraph Montage (Freezing at end)"));
	}

	// 2. �ϴÿ��� â(�) ��ȯ
	// �Ӹ� �� ���� ������ ��ȯ
	FVector SpawnLoc = GetActorLocation() + FVector(0, 0, 2500.0f);
	if (QTEObjectClass)
	{
		ActiveQTEObject = GetWorld()->SpawnActor<AQTEObject>(QTEObjectClass, SpawnLoc, FRotator::ZeroRotator);
		if (ActiveQTEObject)
		{
			ActiveQTEObject->InitializeFalling(this, Duration);
		}
	}
}

void AStage1Boss::HandleQTEResult(bool bSuccess)
{
	if (!HasAuthority()) return;
	
	if (AStageBossGameState* GS = GetWorld()->GetGameState<AStageBossGameState>())
	{
		GS->SetQTEActive(false);
	}
	
	Multicast_FreezeAnimation(false);

	if (bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BOSS] QTE SUCCESS -> EXECUTION"));

		if (ActiveQTEObject) ActiveQTEObject->TriggerSuccess();

		bIsInvincible = false;
		UGameplayStatics::ApplyDamage(this, 999999.f, GetController(), this, UTrueDamageType::StaticClass());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[BOSS] QTE FAILED -> WIPE"));

		if (ActiveQTEObject) ActiveQTEObject->TriggerFailure();

		PerformWipeAttack();
	}
}

void AStage1Boss::EnterPhase2()
{
	if (!HasAuthority()) return;

	UE_LOG(LogTemp, Warning, TEXT(">>> [BOSS] Enter Phase 2 (Enraged) <<<"));

	bPhase2Started = true;
	SetPhase(EBossPhase::Phase2);
	bIsInvincible = false; // 2������ ���� ���� �� ���� ����
}

void AStage1Boss::PerformWipeAttack()
{
	TArray<AActor*> Players;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), Players);

	for (AActor* P : Players)
	{
		if (P && P != this && !P->IsA(AEnemyCharacterBase::StaticClass()))
		{
			UGameplayStatics::ApplyDamage(P, 99999.f, GetController(), this, UTrueDamageType::StaticClass());
		}
	}
}

void AStage1Boss::Multicast_FreezeAnimation_Implementation(bool bFreeze)
{
	UAnimInstance* AnimInst = GetMesh()->GetAnimInstance();
	if (AnimInst && BossData && BossData->QTE_TelegraphMontage)
	{
		if (bFreeze)
		{
			AnimInst->Montage_Pause(BossData->QTE_TelegraphMontage);
		}
		else
		{
			AnimInst->Montage_Resume(BossData->QTE_TelegraphMontage);
		}
	}
}

void AStage1Boss::OnPhaseChanged(EBossPhase NewPhase)
{
	Super::OnPhaseChanged(NewPhase);
	// Gimmick �������� ���� ���� �� ���� ��ȯ
	if (NewPhase == EBossPhase::Gimmick)
	{
		bIsInvincible = true;
		SpawnCrystals();
	}
	else if (NewPhase == EBossPhase::Phase1)
	{
		bIsInvincible = false;
	}
}

void AStage1Boss::OnHealthChanged(float CH, float MH)
{
	// ü�� 70% ���� & �� ���� �̹ߵ� �� ���� & QTE �� �ƴ�
	if (HasAuthority() && !bHasTriggeredDeathWall && !bHasTriggeredQTE_10 && CH > 0 && (CH / MH) <= 0.7f)
	{
		// 50% ������ �� �켱������ �����Ƿ� 50% �̸��̸� �н� (TakeDamage���� ó��)
		if ((CH / MH) > 0.5f)
		{
			bHasTriggeredDeathWall = true;
			StartDeathWallSequence();
		}
	}
}

// ==============================================================================
// [2] �Ϲ� ���� (Melee)
// ==============================================================================

void AStage1Boss::AnimNotify_StartMeleeHold()
{
	if (!HasAuthority()) return;

	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->StopMovement();
		if (UBrainComponent* Brain = AIC->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("HoldLogic"));
		}
	}
	Multicast_SetAttackPlayRate(0.01f);
	GetWorldTimerManager().SetTimer(MeleeAttackTimerHandle, this, &AStage1Boss::ReleaseMeleeAttackHold, 1.5f, false);
}

void AStage1Boss::ReleaseMeleeAttackHold()
{
	if (!HasAuthority()) return;

	Multicast_SetAttackPlayRate(1.0f);

	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AIC->GetBrainComponent())
		{
			Brain->RestartLogic();
		}
	}
}

void AStage1Boss::Multicast_SetAttackPlayRate_Implementation(float NewRate)
{
	UAnimInstance* AnimInst = GetMesh()->GetAnimInstance();
	if (!AnimInst) return;

	UAnimMontage* CurrentMontage = AnimInst->GetCurrentActiveMontage();
	if (CurrentMontage)
	{
		AnimInst->Montage_SetPlayRate(CurrentMontage, NewRate);
		if (FAnimMontageInstance* MontageInst = AnimInst->GetActiveInstanceForMontage(CurrentMontage))
		{
			if (NewRate < 0.5f) MontageInst->bEnableAutoBlendOut = false;
			else MontageInst->bEnableAutoBlendOut = true;
		}
	}
}

void AStage1Boss::AnimNotify_ExecuteMeleeHit()
{
	if (!HasAuthority()) return;

	if (!IsValid(MeleeCollisionBox))
	{
		MeleeCollisionBox = Cast<UBoxComponent>(GetComponentByClass(UBoxComponent::StaticClass()));
		if (!MeleeCollisionBox) return;
	}

	TArray<AActor*> OverlappingActors;
	MeleeCollisionBox->GetOverlappingActors(OverlappingActors, ACharacter::StaticClass());

	for (AActor* Target : OverlappingActors)
	{
		if (Target && Target != this)
		{
			UGameplayStatics::ApplyDamage(Target, MeleeDamageAmount, GetController(), this, UDamageType::StaticClass());
		}
	}
}

void AStage1Boss::DoAttack_Slash()
{
	if (HasAuthority() && BossData && BossData->SlashAttackMontage)
		Multicast_PlayAttackMontage(BossData->SlashAttackMontage, FName("Slash"));
}

void AStage1Boss::DoAttack_Swing()
{
	if (HasAuthority() && BossData && BossData->AttackMontages.Num() > 0)
		Multicast_PlayAttackMontage(BossData->AttackMontages[0]);
}

void AStage1Boss::Multicast_PlayAttackMontage_Implementation(UAnimMontage* MontageToPlay, FName SectionName)
{
	if (!MontageToPlay) return;
	if (HasAuthority()) bIsActionInProgress = true;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance)
	{
		float Duration = AnimInstance->Montage_Play(MontageToPlay);
		if (SectionName != NAME_None) AnimInstance->Montage_JumpToSection(SectionName, MontageToPlay);

		if (HasAuthority())
		{
			FOnMontageEnded EndDelegate;
			EndDelegate.BindUObject(this, &AStage1Boss::OnMontageEnded);
			AnimInstance->Montage_SetEndDelegate(EndDelegate, MontageToPlay);
			if (Duration <= 0.f) bIsActionInProgress = false;
		}
	}
}

void AStage1Boss::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (HasAuthority() && !bIsDeathWallSequenceActive)
	{
		bIsActionInProgress = false;
	}
}

// ==============================================================================
// [3] ������ ���� (Death Wall)
// ==============================================================================

void AStage1Boss::StartDeathWallSequence()
{
	if (!HasAuthority()) return;

	ControlRamparts(true);
	GetWorldTimerManager().SetTimer(RampartTimerHandle, this, &AStage1Boss::RestoreRamparts, 60.0f, false);

	//if (GetCharacterMovement()) GetCharacterMovement()->SetMovementMode(MOVE_None);
	if (GetCharacterMovement()) GetCharacterMovement()->StopMovementImmediately();
	bIsDeathWallSequenceActive = true;
	bIsActionInProgress = true;
	bIsInvincible = true;

	FVector SafeLocation = GetActorLocation() - (GetActorForwardVector() * 250.0f);
	SetActorLocation(SafeLocation, false);

	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->StopMovement();
		if (UBrainComponent* BC = AIC->GetBrainComponent()) BC->PauseLogic(TEXT("DeathWallPattern"));
	}

	Multicast_PlayDeathWallMontage();
}

void AStage1Boss::AnimNotify_ActivateDeathWall()
{
	if (!HasAuthority()) return;
	SpawnDeathWall();
	GetWorldTimerManager().SetTimer(DeathWallTimerHandle, this, &AStage1Boss::FinishDeathWallPattern, 60.0f, false);
}

void AStage1Boss::SpawnDeathWall()
{
	if (!HasAuthority() || !DeathWallClass) return;

	FVector SpawnLoc = IsValid(DeathWallCastLocation) ? DeathWallCastLocation->GetActorLocation() : GetActorLocation() + GetActorForwardVector() * 500.f;
	FRotator SpawnRot = IsValid(DeathWallCastLocation) ? DeathWallCastLocation->GetActorRotation() : GetActorRotation();
	SpawnRot.Yaw += 180.0f;

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADeathWall* NewWall = GetWorld()->SpawnActor<ADeathWall>(DeathWallClass, SpawnLoc, SpawnRot, Params);

	if (NewWall)
	{
		NewWall->SetLifeSpan(110.0f);
		if (GetCapsuleComponent()) GetCapsuleComponent()->IgnoreActorWhenMoving(NewWall, true);
		if (UPrimitiveComponent* WallRoot = Cast<UPrimitiveComponent>(NewWall->GetRootComponent())) WallRoot->IgnoreActorWhenMoving(this, true);
		NewWall->ActivateWall();
	}
}

void AStage1Boss::FinishDeathWallPattern()
{
	if (!HasAuthority()) return;

	if (GetCharacterMovement()) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	bIsDeathWallSequenceActive = false;
	bIsActionInProgress = false;
	bIsInvincible = false;

	GetWorldTimerManager().ClearTimer(DeathWallTimerHandle);

	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* BC = AIC->GetBrainComponent()) BC->RestartLogic();
	}
}

void AStage1Boss::ControlRamparts(bool bSink)
{
	if (!HasAuthority()) return;
	TArray<AActor*> FoundRamparts;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABoss1_Rampart::StaticClass(), FoundRamparts);

	for (AActor* Actor : FoundRamparts)
	{
		if (ABoss1_Rampart* Rampart = Cast<ABoss1_Rampart>(Actor))
			Rampart->Server_SetRampartActive(bSink);
	}
}

void AStage1Boss::RestoreRamparts()
{
	if (!HasAuthority()) return;
	ControlRamparts(false);
	GetWorldTimerManager().ClearTimer(RampartTimerHandle);
}

void AStage1Boss::Multicast_PlayDeathWallMontage_Implementation()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && BossData && BossData->DeathWallSummonMontage)
	{
		if (!AnimInstance->Montage_IsPlaying(BossData->DeathWallSummonMontage))
			PlayAnimMontage(BossData->DeathWallSummonMontage);
	}
	else if (HasAuthority())
	{
		AnimNotify_ActivateDeathWall();
	}
}

void AStage1Boss::OnArrivedAtCastLocation(FAIRequestID RID, EPathFollowingResult::Type R)
{
	if (R == EPathFollowingResult::Success)
	{
		if (AAIController* AIC = Cast<AAIController>(GetController()))
		{
			AIC->ReceiveMoveCompleted.RemoveDynamic(this, &AStage1Boss::OnArrivedAtCastLocation);
			AIC->StopMovement();
		}
		if (DeathWallCastLocation) SetActorRotation(DeathWallCastLocation->GetActorRotation());
		Multicast_PlayDeathWallMontage();
	}
}

// ==============================================================================
// [4] ���: Job Crystal Spawn (������ġ �߰�)
// ==============================================================================

void AStage1Boss::SpawnCrystals()
{
	if (!HasAuthority()) return;

	if (CrystalSpawnPoints.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[BOSS] CrystalSpawnPoints is EMPTY! Please check Blueprint/Level settings."));
		return;
	}

	TArray<EJobType> JobOrder = { EJobType::Titan, EJobType::Striker, EJobType::Mage, EJobType::Paladin };
	RemainingGimmickCount = 0;

	for (int32 i = 0; i < CrystalSpawnPoints.Num(); ++i)
	{
		if (i >= JobOrder.Num()) break;
		if (!CrystalSpawnPoints[i] || !JobCrystalClasses.Contains(JobOrder[i])) continue;

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AJobCrystal* NC = GetWorld()->SpawnActor<AJobCrystal>(
			JobCrystalClasses[JobOrder[i]],
			CrystalSpawnPoints[i]->GetActorLocation(),
			FRotator::ZeroRotator,
			Params);

		if (NC)
		{
			NC->TargetBoss = this;
			NC->RequiredJobType = JobOrder[i];
			RemainingGimmickCount++;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[BOSS] Spawned %d Job Crystals"), RemainingGimmickCount);
}

void AStage1Boss::OnGimmickResolved(int32 GimmickID)
{
	if (!HasAuthority()) return;
	if (--RemainingGimmickCount <= 0)
	{
		// 2������ ���� ���̸� �ٽ� 2������ ����(��������)��, �ƴϸ� 1������ ���·� ����
		if (bPhase2Started) { SetPhase(EBossPhase::Phase2); bIsInvincible = false; }
		else SetPhase(EBossPhase::Phase1);
	}
}

// ==============================================================================
// [5] ������ũ ����
// ==============================================================================

void AStage1Boss::StartSpikePattern()
{
	if (HasAuthority() && BossData && BossData->SpellMontage)
	{
		bIsActionInProgress = true;
		Multicast_PlayAttackMontage(BossData->SpellMontage);
	}
}

void AStage1Boss::ExecuteSpikeSpell()
{
	if (!HasAuthority()) return;
	TArray<AActor*> Found; UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), Found);
	TArray<AActor*> Valid;
	for (AActor* A : Found)
	{
		ACharacter* C = Cast<ACharacter>(A);
		if (C && A != this && C->IsPlayerControlled()) Valid.Add(A);
	}
	if (Valid.Num() == 0) return;

	AActor* T = Valid[FMath::RandRange(0, Valid.Num() - 1)];
	if (T && SpikeTrapClass)
	{
		FVector L = T->GetActorLocation(); FHitResult H;
		L = GetWorld()->LineTraceSingleByChannel(H, L, L - FVector(0, 0, 500), ECC_WorldStatic) ? H.Location : L - FVector(0, 0, 90);
		GetWorld()->SpawnActor<ABossSpikeTrap>(SpikeTrapClass, L, FRotator::ZeroRotator);
	}
}

void AStage1Boss::TrySpawnSpikeAtRandomPlayer()
{
	if (HasAuthority()) { if (SpellMontage) Multicast_PlaySpellMontage(); ExecuteSpikeSpell(); }
}

void AStage1Boss::Multicast_PlaySpellMontage_Implementation()
{
	if (GetMesh() && GetMesh()->GetAnimInstance() && SpellMontage)
		GetMesh()->GetAnimInstance()->Montage_Play(SpellMontage);
}

void AStage1Boss::OnDeathStarted()
{
	Super::OnDeathStarted();
	if (!HasAuthority()) return;
	
	if (AStageBossGameMode* GM = GetWorld()->GetAuthGameMode<AStageBossGameMode>())
	{
		GM->OnBossKilled();
	}
	
	if (TargetPortal)
	{
		TargetPortal->ActivatePortal();
	}
	
	if (BossData && BossData->DeathMontage) Multicast_PlayAttackMontage(BossData->DeathMontage);

	
	// �� ���� �ߴ�
	TArray<AActor*> Walls; UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADeathWall::StaticClass(), Walls);
	for (AActor* W : Walls) W->SetActorTickEnabled(false);
}

