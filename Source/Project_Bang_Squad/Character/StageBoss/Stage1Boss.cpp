// Source/Project_Bang_Squad/Character/StageBoss/Stage1Boss.cpp

#include "Stage1Boss.h"
#include "JobCrystal.h"
#include "DeathWall.h"
#include "BossSpikeTrap.h"
#include "AQTEObject.h" // [QTE] 기믹 오브젝트 헤더

// [QTE] 아키텍처 연동 헤더
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
		SetPhase(EBossPhase::Gimmick);
		if (UHealthComponent* HC = FindComponentByClass<UHealthComponent>())
		{
			HC->OnHealthChanged.AddDynamic(this, &AStage1Boss::OnHealthChanged);
		}

		// 이름 충돌 방지: MeleeCollisionBox 변수에 블루프린트의 BoxComponent를 찾아 연결
		if (!IsValid(MeleeCollisionBox))
		{
			MeleeCollisionBox = Cast<UBoxComponent>(GetComponentByClass(UBoxComponent::StaticClass()));
		}
	}
}

// ==============================================================================
// [1] 데미지 처리 및 페이즈 전환 (QTE 연동)
// ==============================================================================

float AStage1Boss::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// [무적 처리 1] 기존 패턴(성벽) 중일 때
	if (!HasAuthority() || bIsDeathWallSequenceActive) return 0.0f;

	// [무적 처리 2] QTE 패턴(창 떨어지기) 중일 때 (GameState 확인)
	if (AStageBossGameState* GS = GetWorld()->GetGameState<AStageBossGameState>())
	{
		if (GS->bIsQTEActive) return 0.0f;
	}

	// 팀킬 방지
	AActor* Attacker = DamageCauser;
	if (EventInstigator && EventInstigator->GetPawn()) Attacker = EventInstigator->GetPawn();
	if (Attacker && (Attacker == this || Attacker->IsA(AEnemyCharacterBase::StaticClass()))) return 0.0f;

	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (ActualDamage <= 0.0f) return 0.0f;

	// 페이즈 전환 및 기믹 발동 체크
	if (UHealthComponent* HC = FindComponentByClass<UHealthComponent>())
	{
		HC->ApplyDamage(ActualDamage);

		// [UI 동기화] 보스 체력 바 갱신을 위해 GameState에 알림
		if (AStageBossGameState* GS = GetWorld()->GetGameState<AStageBossGameState>())
		{
			GS->UpdateBossHealth(HC->GetHealth(), HC->MaxHealth);
		}

		float HpRatio = HC->GetHealth() / HC->MaxHealth;
		float Threshold = (BossData ? BossData->GimmickThresholdRatio : 0.5f);

		// [핵심 변경] 체력 조건 만족 시 -> 바로 2페이즈가 아니라 'QTE 기믹' 시작
		if (HasAuthority() && !bHasTriggeredGimmick && HpRatio <= Threshold && CurrentPhase == EBossPhase::Phase1)
		{
			bHasTriggeredGimmick = true;

			// GameMode에게 "심판을 시작해달라"고 요청
			if (AStageBossGameMode* GM = GetWorld()->GetAuthGameMode<AStageBossGameMode>())
			{
				GM->TriggerSpearQTE(this);
			}
		}
	}
	return ActualDamage;
}

// GameMode가 호출: 비주얼 연출 시작
void AStage1Boss::PlayQTEVisuals(float Duration)
{
	if (!HasAuthority()) return;

	// 1. 전조 동작 몽타주 재생 (멀티캐스트) - [추가된 부분]
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

// GameMode가 호출: 결과 처리
void AStage1Boss::HandleQTEResult(bool bSuccess)
{
	if (!HasAuthority()) return;

	if (bSuccess)
	{
		// 성공 -> 창 폭발 연출 -> 2페이즈 진입
		if (ActiveQTEObject) ActiveQTEObject->TriggerSuccess();
		EnterPhase2();
	}
	else
	{
		// 실패 -> 창 충돌 연출 -> 전멸기
		if (ActiveQTEObject) ActiveQTEObject->TriggerFailure();
		PerformWipeAttack();
	}
}

void AStage1Boss::EnterPhase2()
{
	if (!HasAuthority()) return;

	UE_LOG(LogTemp, Warning, TEXT(">>> [BOSS] Enter Phase 2 <<<"));

	bPhase2Started = true;
	SetPhase(EBossPhase::Phase2);

	// 페이즈 전환 시 잠깐 무적 해제 (또는 기획에 따라 유지)
	bIsInvincible = true;

	SpawnCrystals(); // 2페이즈 시작 시 수정 소환
}

void AStage1Boss::PerformWipeAttack()
{
	UE_LOG(LogTemp, Error, TEXT(">>> [BOSS] WIPE ATTACK TRIGGERED! <<<"));

	// 모든 플레이어에게 즉사급 데미지
	TArray<AActor*> Players;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), Players);

	for (AActor* P : Players)
	{
		if (P && P != this)
		{
			// TrueDamageType을 사용하여 방어 무시 데미지 적용
			UGameplayStatics::ApplyDamage(P, 99999.f, GetController(), this, UTrueDamageType::StaticClass());
		}
	}
}

void AStage1Boss::OnPhaseChanged(EBossPhase NewPhase)
{
	Super::OnPhaseChanged(NewPhase);
	if (NewPhase == EBossPhase::Gimmick) { bIsInvincible = true; SpawnCrystals(); }
	else if (NewPhase == EBossPhase::Phase1) bIsInvincible = false;
}

void AStage1Boss::OnHealthChanged(float CH, float MH)
{
	// 체력 70% 이하 & 아직 벽 패턴 안 썼으면 발동
	if (HasAuthority() && !bHasTriggeredDeathWall && CH > 0 && (CH / MH) <= 0.7f)
	{
		bHasTriggeredDeathWall = true;
		StartDeathWallSequence();
	}
}

// ==============================================================================
// [2] 일반 공격 (Melee & Animation Control)
// ==============================================================================

// 1. 1.5초간 멈추기
void AStage1Boss::AnimNotify_StartMeleeHold()
{
	if (!HasAuthority()) return;

	// AI 이동 금지
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->StopMovement();
		if (UBrainComponent* Brain = AIC->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("HoldLogic"));
		}
	}

	// 0.01f로 설정하여 애니메이션이 완전히 멈추지 않게 함
	Multicast_SetAttackPlayRate(0.01f);

	// 1.5초 뒤 재개
	GetWorldTimerManager().SetTimer(MeleeAttackTimerHandle, this, &AStage1Boss::ReleaseMeleeAttackHold, 1.5f, false);
}

// 2. 1.5초 뒤 재개
void AStage1Boss::ReleaseMeleeAttackHold()
{
	if (!HasAuthority()) return;

	// 정상 속도로 복구
	Multicast_SetAttackPlayRate(1.0f);

	// AI 다시 켜기
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AIC->GetBrainComponent())
		{
			Brain->RestartLogic();
		}
	}
}

// 3. 멀티캐스트 (속도 조절)
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
			// 속도가 느릴 때는 블렌드 아웃 방지
			if (NewRate < 0.5f)
			{
				MontageInst->bEnableAutoBlendOut = false;
			}
			else
			{
				MontageInst->bEnableAutoBlendOut = true;
			}
		}
	}
}

// [노티파이 2] 실제 타격 지점
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
			UGameplayStatics::ApplyDamage(
				Target,
				MeleeDamageAmount,
				GetController(),
				this,
				UDamageType::StaticClass()
			);
		}
	}
}

// AI Controller 호출용 함수
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
// [3] 죽음의 성벽 (Death Wall) & Rampart
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
// [4] 기믹: Job Crystal Spawn
// ==============================================================================

void AStage1Boss::SpawnCrystals()
{
	if (!HasAuthority()) return;
	TArray<EJobType> JobOrder = { EJobType::Titan, EJobType::Striker, EJobType::Mage, EJobType::Paladin };
	RemainingGimmickCount = 0;

	for (int32 i = 0; i < CrystalSpawnPoints.Num(); ++i)
	{
		if (i >= JobOrder.Num()) break;
		if (!JobCrystalClasses.Contains(JobOrder[i]) || !CrystalSpawnPoints[i]) continue;

		if (AJobCrystal* NC = GetWorld()->SpawnActor<AJobCrystal>(JobCrystalClasses[JobOrder[i]], CrystalSpawnPoints[i]->GetActorLocation(), FRotator::ZeroRotator))
		{
			NC->TargetBoss = this;
			NC->RequiredJobType = JobOrder[i];
			RemainingGimmickCount++;
		}
	}
}

void AStage1Boss::OnGimmickResolved(int32 GimmickID)
{
	if (!HasAuthority()) return;
	if (--RemainingGimmickCount <= 0)
	{
		if (bPhase2Started) { SetPhase(EBossPhase::Phase2); bIsInvincible = false; }
		else SetPhase(EBossPhase::Phase1);
	}
}

// ==============================================================================
// [5] 기타 스킬 (Spike Pattern)
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