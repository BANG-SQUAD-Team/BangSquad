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

// ==============================================================================
// [1] 데미지 처리 및 기믹(100, 50, 0) 발동 로직 (핵심)
// ==============================================================================

// Source/Project_Bang_Squad/Character/StageBoss/Stage1Boss.cpp

float AStage1Boss::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// [핵심 수정] 트루 데미지(처형)인지 먼저 식별합니다.
	// 트루 데미지는 무적, QTE 상태, 아군 여부와 상관없이 무조건 뚫고 들어가야 합니다.
	bool bIsTrueDamage = (DamageEvent.DamageTypeClass == UTrueDamageType::StaticClass());

	// 1. [방어 로직] 트루 데미지가 "아닐 때만" 방어 기제가 작동합니다.
	if (!bIsTrueDamage)
	{
		// 서버 권한 없음 OR QTE 진행 중 OR 무적 상태라면 데미지 0 (일반 공격 무효화)
		if (!HasAuthority() || bHasTriggeredQTE_10 || bIsInvincible) return 0.0f;

		// GameState 레벨의 QTE 컷신 중인지 체크
		AStageBossGameState* GS = GetWorld()->GetGameState<AStageBossGameState>();
		if (GS && GS->bIsQTEActive) return 0.0f;

		// 팀킬 방지
		AActor* Attacker = DamageCauser;
		if (EventInstigator && EventInstigator->GetPawn()) Attacker = EventInstigator->GetPawn();
		if (Attacker && (Attacker == this || Attacker->IsA(AEnemyCharacterBase::StaticClass()))) return 0.0f;
	}

	// 2. [체력 잠금 및 기믹 발동]
	float ActualDamage = DamageAmount;

	if (UHealthComponent* HC = FindComponentByClass<UHealthComponent>())
	{
		float CurrentHP = HC->GetHealth();
		float MaxHP = HC->MaxHealth;

		// --- [기믹 1] 체력 50% 패턴 (기존 유지) ---
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

		// --- [기믹 2] 체력 1 이하 보호 (QTE 진입) ---
		// [수정] 트루 데미지가 아닐 때만 보호 (트루 데미지면 그냥 통과해서 죽게 둠)
		if (!bIsTrueDamage && (CurrentHP - DamageAmount) <= 1.0f && !bHasTriggeredQTE_10)
		{
			// 체력을 1.0f까지만 깎이도록 데미지 조절
			ActualDamage = FMath::Max(0.0f, CurrentHP - 1.0f);

			// 조절된 데미지 적용
			Super::TakeDamage(ActualDamage, DamageEvent, EventInstigator, DamageCauser);

			// QTE 모드 시작 (무적 및 정지)
			bHasTriggeredQTE_10 = true;
			bIsInvincible = true;

			if (AAIController* AIC = Cast<AAIController>(GetController())) AIC->StopMovement();

			// GameMode에 QTE 시작 요청
			if (AStageBossGameMode* GM = GetWorld()->GetAuthGameMode<AStageBossGameMode>())
			{
				GM->TriggerSpearQTE(this);
				UE_LOG(LogTemp, Warning, TEXT("[BOSS] HP 1 Reached! Finale QTE Triggered!"));
			}

			// 여기서 리턴하여 1HP 상태 유지
			return ActualDamage;
		}
	}

	// 3. 실제 데미지 적용
	// 트루 데미지(99999)는 위 if문들을 모두 통과하여 여기에 도달 -> 즉사
	return Super::TakeDamage(ActualDamage, DamageEvent, EventInstigator, DamageCauser);
}

// GameMode 호출: 비주얼 연출 (서버 -> 멀티캐스트)
void AStage1Boss::PlayQTEVisuals(float Duration)
{
	if (!HasAuthority()) return;

	// 1. 전조 동작 몽타주 재생 (지쳐서 헐떡임 등)
	if (BossData && BossData->QTE_TelegraphMontage)
	{
		// 몽타주 재생
		Multicast_PlayAttackMontage(BossData->QTE_TelegraphMontage);

		// 몽타주 끝자락에서 정지(Freeze) 시키기 위한 타이머
		// (몽타주 길이에 맞춰 적절히 조절 필요, 여기선 예시로 90% 지점)
		float MontageLength = BossData->QTE_TelegraphMontage->GetPlayLength();
		FTimerHandle FreezeTimer;
		GetWorldTimerManager().SetTimer(FreezeTimer, [this]()
			{
				Multicast_FreezeAnimation(true);
			}, MontageLength - 0.1f, false);

		UE_LOG(LogTemp, Log, TEXT("[BOSS] Playing QTE Telegraph Montage (Freezing at end)"));
	}

	// 2. 하늘에서 창(운석) 소환
	// 머리 위 높은 곳에서 소환
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

// GameMode 호출: 결과 처리
void AStage1Boss::HandleQTEResult(bool bSuccess)
{
	if (!HasAuthority()) return;

	// 애니메이션 다시 재생
	Multicast_FreezeAnimation(false);

	if (bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BOSS] QTE SUCCESS -> EXECUTION"));

		// QTE 물체 성공 연출 (폭발 등)
		if (ActiveQTEObject) ActiveQTEObject->TriggerSuccess();

		// 무적 해제 및 트루 데미지로 즉사
		bIsInvincible = false;
		UGameplayStatics::ApplyDamage(this, 999999.f, GetController(), this, UTrueDamageType::StaticClass());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[BOSS] QTE FAILED -> WIPE"));

		// QTE 물체 실패 연출 (꽝)
		if (ActiveQTEObject) ActiveQTEObject->TriggerFailure();

		// 전멸기 실행
		PerformWipeAttack();
	}
}

void AStage1Boss::EnterPhase2()
{
	if (!HasAuthority()) return;

	UE_LOG(LogTemp, Warning, TEXT(">>> [BOSS] Enter Phase 2 (Enraged) <<<"));

	bPhase2Started = true;
	SetPhase(EBossPhase::Phase2);
	bIsInvincible = false; // 2페이즈 전투 시작 시 무적 해제
}

void AStage1Boss::PerformWipeAttack()
{
	// 플레이어 전원에게 트루 데미지 즉사
	TArray<AActor*> Players;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), Players);

	for (AActor* P : Players)
	{
		// 나 자신(보스)이나 몬스터는 제외하고 플레이어만
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
	// 체력 70% 이하 & 벽 패턴 미발동 시 실행 & QTE 중 아님
	if (HasAuthority() && !bHasTriggeredDeathWall && !bHasTriggeredQTE_10 && CH > 0 && (CH / MH) <= 0.7f)
	{
		// 50% 패턴이 더 우선순위가 높으므로 50% 미만이면 패스 (TakeDamage에서 처리)
		if ((CH / MH) > 0.5f)
		{
			bHasTriggeredDeathWall = true;
			StartDeathWallSequence();
		}
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