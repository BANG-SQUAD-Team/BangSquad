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
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Net/UnrealNetwork.h"

AStage1Boss::AStage1Boss()
{
	// 생성자 로직
}

void AStage1Boss::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		// [수정] 조우 즉시(100) 크리스탈 기믹 실행
		// 시작하자마자 SetPhase(Gimmick)을 호출하여 무적 상태로 만들고 수정을 소환합니다.
		if (!bHasTriggeredCrystal_100)
		{
			bHasTriggeredCrystal_100 = true;
			SetPhase(EBossPhase::Gimmick); // 내부적으로 SpawnCrystals() 호출됨
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
// [1] 데미지 처리 및 기믹(100, 50, 10) 발동 로직
// ==============================================================================

// Source/Project_Bang_Squad/Character/StageBoss/Stage1Boss.cpp

float AStage1Boss::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// 1. [방어 로직] 서버 권한, 성벽 패턴 중, 무적, QTE 중이면 데미지 무시
	// 주의: 70% 패턴(DeathWall) 중에는 데미지를 안 입어야 패턴이 안 꼬입니다.
	if (!HasAuthority() || bIsDeathWallSequenceActive || bIsInvincible) return 0.0f;

	AStageBossGameState* GS = GetWorld()->GetGameState<AStageBossGameState>();
	if (GS && GS->bIsQTEActive) return 0.0f;

	// 2. [팀킬 방지]
	AActor* Attacker = DamageCauser;
	if (EventInstigator && EventInstigator->GetPawn()) Attacker = EventInstigator->GetPawn();
	if (Attacker && (Attacker == this || Attacker->IsA(AEnemyCharacterBase::StaticClass()))) return 0.0f;

	// ==============================================================================
	// [수정 핵심 1] 중복 데미지 제거
	// Super::TakeDamage를 호출하면 내부적으로 ApplyDamage가 실행됩니다.
	// 따라서 반환받은 ActualDamage를 또 ApplyDamage 하면 안 됩니다.
	// ==============================================================================
	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (ActualDamage <= 0.0f) return 0.0f;

	// 3. 체력 및 기믹 체크
	if (UHealthComponent* HC = FindComponentByClass<UHealthComponent>())
	{
		float CurrentHP = HC->GetHealth();
		float MaxHP = HC->MaxHealth;
		float HpRatio = CurrentHP / MaxHP;

		// [디버그] 체력 상황 모니터링
		// UE_LOG(LogTemp, Log, TEXT("[BOSS] HP: %.0f / %.0f (Ratio: %.2f)"), CurrentHP, MaxHP, HpRatio);

		if (GS) GS->UpdateBossHealth(CurrentHP, MaxHP);

		// ==========================================================================
		// [수정 핵심 2] 패턴 충돌 방지 (70% vs 50%)
		// 70% 패턴은 OnHealthChanged에서 발동되므로 여기서는 신경 쓰지 않아도 되지만,
		// 만약 데미지가 너무 커서 70%를 건너뛰고 50%가 되었다면?
		// -> 우선순위에 따라 '성벽'을 무시하고 '크리스탈'을 가거나, 순차적으로 처리해야 함.
		// -> 여기서는 50%가 더 중요하므로, 50% 조건 만족 시 즉시 기믹으로 전환합니다.
		// ==========================================================================

		// --- [기믹 1] 체력 50% 이하: 크리스탈 재소환 ---
		if (!bHasTriggeredCrystal_50 && HpRatio <= 0.5f)
		{
			// 혹시 70% 패턴이 켜져있다면 강제 종료 시키고 전환
			if (bIsDeathWallSequenceActive)
			{
				FinishDeathWallPattern();
				UE_LOG(LogTemp, Warning, TEXT("[BOSS] Force Stopping DeathWall for 50%% Gimmick"));
			}

			// 페이즈 확인 (Phase1일 때만 발동)
			if (CurrentPhase == EBossPhase::Phase1)
			{
				bHasTriggeredCrystal_50 = true;
				SetPhase(EBossPhase::Gimmick); // -> SpawnCrystals() 호출됨
				UE_LOG(LogTemp, Warning, TEXT("[BOSS] 50%% HP Reached: Starting Crystal Gimmick!"));

				// 기믹 페이즈로 가면 무적이 되므로, 추가 데미지 로직 방지
				return ActualDamage;
			}
		}

		// --- [기믹 2] 체력 0 도달: 피니시 QTE 발동 ---
		if (CurrentHP <= 0.0f && !bHasTriggeredQTE_10)
		{
			// 1. 죽음 유예 (데미지 롤백 느낌으로 체력 1 설정)
			// 참고: 이미 Super에서 깎였으므로 ApplyDamage가 아닌 SetHealth 개념이 필요하지만,
			// HealthComponent 수정 없이 하려면 다음 프레임에 회복하거나, 
			// 여기서 그냥 로직적으로만 처리하고 GameMode에 넘깁니다.
			// (가장 깔끔한 건 아까 알려드린 'DamageAmount 조절' 방식이지만, 
			//  지금은 Super가 이미 불렸으므로 로직만 실행합니다.)

			bHasTriggeredQTE_10 = true;

			// QTE 시작 요청
			if (AStageBossGameMode* GM = GetWorld()->GetAuthGameMode<AStageBossGameMode>())
			{
				GM->TriggerSpearQTE(this);
				UE_LOG(LogTemp, Warning, TEXT("[BOSS] HP 0 Reached! Finale QTE Triggered!"));
			}
		}
	}

	return ActualDamage;
}

// GameMode 호출: 비주얼 연출
void AStage1Boss::PlayQTEVisuals(float Duration)
{
	if (!HasAuthority()) return;

	// 1. 전조 동작 몽타주 재생
	if (BossData && BossData->QTE_TelegraphMontage)
	{
		Multicast_PlayAttackMontage(BossData->QTE_TelegraphMontage);
		UE_LOG(LogTemp, Log, TEXT("[BOSS] Playing QTE Telegraph Montage"));
	}

	// 2. 하늘에서 창(운석) 소환
	FVector SpawnLoc = GetActorLocation() + FVector(0, 0, 2000.0f);
	if (QTEObjectClass)
	{
		ActiveQTEObject = GetWorld()->SpawnActor<AQTEObject>(QTEObjectClass, SpawnLoc, FRotator::ZeroRotator);
		if (ActiveQTEObject)
		{
			ActiveQTEObject->InitializeFalling(this, Duration);
		}
	}
}

// GameMode 호출: 결과 처리
void AStage1Boss::HandleQTEResult(bool bSuccess)
{
	if (!HasAuthority()) return;

	if (bSuccess)
	{
		// 성공 -> 창 폭발 -> 2페이즈(광폭화)
		if (ActiveQTEObject) ActiveQTEObject->TriggerSuccess();
		EnterPhase2();
	}
	else
	{
		// 실패 -> 전멸기
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
	bIsInvincible = false; // 2페이즈 전투 시작 시 무적 해제 (필요 시 조정)

	// 기획에 따라 2페이즈 시작 시 수정을 또 소환할지 결정 (현재는 생략)
	// SpawnCrystals(); 
}

void AStage1Boss::PerformWipeAttack()
{
	UE_LOG(LogTemp, Error, TEXT(">>> [BOSS] WIPE ATTACK TRIGGERED! <<<"));

	TArray<AActor*> Players;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), Players);

	for (AActor* P : Players)
	{
		if (P && P != this)
		{
			UGameplayStatics::ApplyDamage(P, 99999.f, GetController(), this, UTrueDamageType::StaticClass());
		}
	}
}

void AStage1Boss::OnPhaseChanged(EBossPhase NewPhase)
{
	Super::OnPhaseChanged(NewPhase);
	// Gimmick 페이즈일 때만 무적 및 수정 소환
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
	// 체력 70 이하 & 벽 패턴 미발동 시 실행
	if (HasAuthority() && !bHasTriggeredDeathWall && CH > 0 && (CH / MH) <= 0.7f)
	{
		bHasTriggeredDeathWall = true;
		StartDeathWallSequence();
	}
}

// ==============================================================================
// [2] 일반 공격 (Melee)
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
// [3] 죽음의 성벽 (Death Wall)
// ==============================================================================

void AStage1Boss::StartDeathWallSequence()
{
	if (!HasAuthority()) return;

	ControlRamparts(true);
	GetWorldTimerManager().SetTimer(RampartTimerHandle, this, &AStage1Boss::RestoreRamparts, 105.0f, false);

	if (GetCharacterMovement()) GetCharacterMovement()->SetMovementMode(MOVE_None);
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
// [4] 기믹: Job Crystal Spawn (안전장치 추가)
// ==============================================================================

void AStage1Boss::SpawnCrystals()
{
	if (!HasAuthority()) return;

	// [크래쉬 방지] 스폰 포인트가 설정되지 않았으면 중단
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
		// 2페이즈 진행 중이면 다시 2페이즈 상태(무적해제)로, 아니면 1페이즈 상태로 복귀
		if (bPhase2Started) { SetPhase(EBossPhase::Phase2); bIsInvincible = false; }
		else SetPhase(EBossPhase::Phase1);
	}
}

// ==============================================================================
// [5] 스파이크 패턴
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
	if (BossData && BossData->DeathMontage) Multicast_PlayAttackMontage(BossData->DeathMontage);

	// 벽 패턴 중단
	TArray<AActor*> Walls; UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADeathWall::StaticClass(), Walls);
	for (AActor* W : Walls) W->SetActorTickEnabled(false);
}