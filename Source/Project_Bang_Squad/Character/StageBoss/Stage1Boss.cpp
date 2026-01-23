// Source/Project_Bang_Squad/Character/StageBoss/Stage1Boss.cpp
#include "Stage1Boss.h"
#include "JobCrystal.h"
#include "DeathWall.h"
#include "Project_Bang_Squad/Projectile/SlashProjectile.h"
#include "Project_Bang_Squad/Core/TrueDamageType.h"
#include "Project_Bang_Squad/Core/BSGameInstance.h"
#include "Project_Bang_Squad/Game/Stage/StageGameMode.h"
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h"
#include "Engine/TargetPoint.h"
#include "BossSpikeTrap.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "BrainComponent.h"
#include "Net/UnrealNetwork.h"

// ============================================================================
// [Constructor & BeginPlay]
// ============================================================================

AStage1Boss::AStage1Boss()
{
}

void AStage1Boss::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority() && !BossData)
    {
        UE_LOG(LogTemp, Error, TEXT("[AStage1Boss] BossData is MISSING!"));
    }

    if (HasAuthority())
    {
        SetPhase(EBossPhase::Gimmick);

        UHealthComponent* HC = FindComponentByClass<UHealthComponent>();
        if (HC)
        {
            // 체력 변경 이벤트 바인딩
            HC->OnHealthChanged.AddDynamic(this, &AStage1Boss::OnHealthChanged);
        }
    }
}

// ============================================================================
// [Phase & Damage Logic]
// ============================================================================

float AStage1Boss::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (!HasAuthority()) return 0.0f;

    // ★ [State Lock] 데스 월 패턴 중에는 완전 무적 상태 유지
    // 패턴 중에 피격 몽타주가 재생되면 스킬이 끊기므로 데미지를 0으로 처리
    if (bIsDeathWallSequenceActive)
    {
        return 0.0f;
    }

    // 팀킬 방지 로직
    AActor* Attacker = DamageCauser;
    if (EventInstigator && EventInstigator->GetPawn()) Attacker = EventInstigator->GetPawn();
    else if (DamageCauser && DamageCauser->GetOwner()) Attacker = DamageCauser->GetOwner();

    if (Attacker)
    {
        if (Attacker == this) return 0.0f;
        if (Attacker->IsA(AEnemyCharacterBase::StaticClass())) return 0.0f;
    }

    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    if (ActualDamage <= 0.0f) return 0.0f;

    UHealthComponent* HC = FindComponentByClass<UHealthComponent>();
    if (HC && !HC->IsDead())
    {
        HC->ApplyDamage(ActualDamage);
    }
    else
    {
        return 0.0f;
    }

    if (HC && HC->MaxHealth > 0.0f)
    {
        float HpRatio = HC->GetHealth() / HC->MaxHealth;
        float Threshold = BossData ? BossData->GimmickThresholdRatio : 0.5f;

        if (HpRatio <= Threshold && !bPhase2Started && CurrentPhase == EBossPhase::Phase1)
        {
            EnterPhase2();
        }
    }

    return ActualDamage;
}

void AStage1Boss::EnterPhase2()
{
    if (!HasAuthority()) return;

    bPhase2Started = true;
    SetPhase(EBossPhase::Phase2);
    bIsInvincible = true;
    SpawnCrystals();

    // [참고] SpawnDeathWall()은 여기서 호출하지 않음 (70% 시퀀스에서 이미 처리됨)

    UE_LOG(LogTemp, Warning, TEXT("=== ENTERING PHASE 2 ==="));
}

void AStage1Boss::OnDeathStarted()
{
    Super::OnDeathStarted();
    if (!HasAuthority()) return;

    if (BossData && BossData->DeathMontage)
    {
        Multicast_PlayAttackMontage(BossData->DeathMontage);
    }

    // 모든 벽 정지
    TArray<AActor*> Walls;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADeathWall::StaticClass(), Walls);
    for (AActor* Wall : Walls)
    {
        Wall->SetActorTickEnabled(false);
    }
}

// ============================================================================
// [Gimmick: Crystals]
// ============================================================================

void AStage1Boss::OnPhaseChanged(EBossPhase NewPhase)
{
    Super::OnPhaseChanged(NewPhase);

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

void AStage1Boss::SpawnCrystals()
{
    if (!HasAuthority()) return;
    if (CrystalSpawnPoints.Num() == 0) return;

    TArray<EJobType> JobOrder = { EJobType::Titan, EJobType::Striker, EJobType::Mage, EJobType::Paladin };
    RemainingGimmickCount = 0;

    for (int32 i = 0; i < CrystalSpawnPoints.Num(); ++i)
    {
        if (i >= JobOrder.Num()) break;
        EJobType CurrentJob = JobOrder[i];

        if (!JobCrystalClasses.Contains(CurrentJob)) continue;
        TSubclassOf<AJobCrystal> TargetClass = JobCrystalClasses[CurrentJob];
        if (!TargetClass) continue;

        ATargetPoint* SpawnPoint = CrystalSpawnPoints[i];
        if (!IsValid(SpawnPoint)) continue;

        FVector SpawnLoc = SpawnPoint->GetActorLocation();
        FRotator SpawnRot = FRotator::ZeroRotator;

        FActorSpawnParameters Params;
        Params.Owner = this;
        Params.Instigator = this;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

        AJobCrystal* NewCrystal = GetWorld()->SpawnActor<AJobCrystal>(TargetClass, SpawnLoc, SpawnRot, Params);
        if (NewCrystal)
        {
            NewCrystal->TargetBoss = this;
            NewCrystal->RequiredJobType = CurrentJob;
            RemainingGimmickCount++;
        }
    }
}

void AStage1Boss::OnGimmickResolved(int32 GimmickID)
{
    if (!HasAuthority()) return;
    RemainingGimmickCount--;

    if (RemainingGimmickCount <= 0)
    {
        if (bPhase2Started)
        {
            SetPhase(EBossPhase::Phase2);
            bIsInvincible = false;
        }
        else
        {
            SetPhase(EBossPhase::Phase1);
        }
    }
}

// ============================================================================
// [Combat]
// ============================================================================

void AStage1Boss::DoAttack_Slash()
{
    if (!HasAuthority()) return;
    if (BossData && BossData->SlashAttackMontage)
        Multicast_PlayAttackMontage(BossData->SlashAttackMontage, FName("Slash"));
}

void AStage1Boss::DoAttack_Swing()
{
    if (!HasAuthority()) return;
    if (BossData && BossData->AttackMontages.Num() > 0)
        Multicast_PlayAttackMontage(BossData->AttackMontages[0]);
}

void AStage1Boss::Multicast_PlayAttackMontage_Implementation(UAnimMontage* MontageToPlay, FName SectionName)
{
    if (MontageToPlay) PlayAnimMontage(MontageToPlay, 1.0f, SectionName);
}

void AStage1Boss::AnimNotify_SpawnSlash()
{
    if (!HasAuthority()) return;
    if (!BossData || !BossData->SlashProjectileClass) return;

    FVector SpawnLocation = GetActorLocation() + GetActorForwardVector() * 150.0f;
    FRotator SpawnRotation = GetActorRotation();
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;

    GetWorld()->SpawnActor<ASlashProjectile>(BossData->SlashProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);
}

void AStage1Boss::AnimNotify_CheckMeleeHit()
{
    if (!HasAuthority()) return;
    FVector TraceStart = GetActorLocation();

    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(this);
    TArray<AActor*> OutActors;

    bool bHit = UKismetSystemLibrary::SphereOverlapActors(GetWorld(), TraceStart, MeleeAttackRadius, ObjectTypes, nullptr, ActorsToIgnore, OutActors);
    for (AActor* HitActor : OutActors)
    {
        if (HitActor && HitActor != this)
        {
            UGameplayStatics::ApplyDamage(HitActor, MeleeDamageAmount, GetController(), this, UDamageType::StaticClass());
        }
    }
}

// ============================================================================
// [QTE]
// ============================================================================

void AStage1Boss::StartSpearQTE()
{
    if (!HasAuthority()) return;
    QTEProgressMap.Empty();

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (APlayerController* PC = It->Get())
            if (PC->GetPawn()) QTEProgressMap.Add(PC, FPlayerQTEStatus());
    }

    if (QTEProgressMap.Num() == 0) return;

    Multicast_SetQTEWidget(true);
    GetWorldTimerManager().SetTimer(QTETimerHandle, this, &AStage1Boss::EndSpearQTE, QTEDuration, false);
}

void AStage1Boss::Server_SubmitQTEInput_Implementation(APlayerController* PlayerController)
{
    if (!GetWorldTimerManager().IsTimerActive(QTETimerHandle)) return;
    if (!QTEProgressMap.Contains(PlayerController)) return;

    FPlayerQTEStatus& Status = QTEProgressMap[PlayerController];
    Status.PressCount++;
    if (Status.PressCount > QTEGoalCount) Status.bFailed = true;
}

void AStage1Boss::EndSpearQTE()
{
    if (!HasAuthority()) return;
    Multicast_SetQTEWidget(false);

    bool bAllSuccess = true;
    for (auto& Pair : QTEProgressMap)
    {
        if (Pair.Value.PressCount != QTEGoalCount || Pair.Value.bFailed)
        {
            bAllSuccess = false;
            break;
        }
    }

    if (bAllSuccess)
    {
        if (!bPhase2Started) EnterPhase2();
        else SpawnDeathWall();
    }
    else
    {
        PerformWipeAttack();
    }
}

void AStage1Boss::PerformWipeAttack()
{
    for (auto& Pair : QTEProgressMap)
    {
        if (APlayerController* PC = Pair.Key)
        {
            if (APawn* TargetPawn = PC->GetPawn())
            {
                UGameplayStatics::ApplyDamage(TargetPawn, 1000.0f, GetController(), this, UTrueDamageType::StaticClass());
            }
        }
    }
}

void AStage1Boss::Multicast_SetQTEWidget_Implementation(bool bVisible)
{
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
    {
        if (bVisible) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("[UI] QTE!"));
        else GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("[UI] QTE END"));
    }
}

void AStage1Boss::SpawnDeathWall()
{
    if (!HasAuthority() || !DeathWallClass) return;

    FActorSpawnParameters Params;
    Params.Owner = this;

    // 벽은 보스의 현재 회전 방향(타겟 포인트 방향)으로 생성됩니다.
    FRotator SpawnRot = GetActorRotation();
    FVector SpawnLoc = GetActorTransform().TransformPosition(WallSpawnLocation);

    ADeathWall* NewWall = GetWorld()->SpawnActor<ADeathWall>(DeathWallClass, SpawnLoc, SpawnRot, Params);
    if (NewWall)
    {
        NewWall->ActivateWall();
    }
}

void AStage1Boss::TrySpawnSpikeAtRandomPlayer()
{
    if (!HasAuthority()) return;
    if (SpellMontage) Multicast_PlaySpellMontage();

    TArray<AActor*> FoundPlayers;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), FoundPlayers);

    TArray<AActor*> ValidPlayers;
    for (AActor* Actor : FoundPlayers)
    {
        if (Actor != this && !Actor->IsHidden()) ValidPlayers.Add(Actor);
    }

    if (ValidPlayers.Num() == 0) return;

    int32 RandomIndex = FMath::RandRange(0, ValidPlayers.Num() - 1);
    AActor* TargetPlayer = ValidPlayers[RandomIndex];

    if (TargetPlayer && SpikeTrapClass)
    {
        FVector SpawnLocation = TargetPlayer->GetActorLocation();
        SpawnLocation.Z -= 90.0f;
        FRotator SpawnRotation = FRotator::ZeroRotator;
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.Instigator = this;

        GetWorld()->SpawnActor<ABossSpikeTrap>(SpikeTrapClass, SpawnLocation, SpawnRotation, SpawnParams);
    }
}

void AStage1Boss::Multicast_PlaySpellMontage_Implementation()
{
    if (GetMesh() && GetMesh()->GetAnimInstance() && SpellMontage)
    {
        GetMesh()->GetAnimInstance()->Montage_Play(SpellMontage);
    }
}

// ============================================================================
// [Spike Pattern]
// ============================================================================

void AStage1Boss::StartSpikePattern()
{
    if (!HasAuthority()) return;
    if (!BossData || !BossData->SpellMontage) return;

    if (GetMesh()) GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

    StopAnimMontage(); // 기존 모션 캔슬

    float Duration = PlayAnimMontage(BossData->SpellMontage);
    if (Duration <= 0.0f)
    {
        FTimerHandle FallbackHandle;
        GetWorldTimerManager().SetTimer(FallbackHandle, this, &AStage1Boss::ExecuteSpikeSpell, 0.5f, false);
    }
}

void AStage1Boss::ExecuteSpikeSpell()
{
    if (!HasAuthority()) return;

    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), FoundActors);

    TArray<AActor*> ValidPlayers;
    for (AActor* Actor : FoundActors)
    {
        ACharacter* Char = Cast<ACharacter>(Actor);
        if (!Char) continue;
        if (Actor != this && !Actor->IsHidden() && Char->IsPlayerControlled())
        {
            UHealthComponent* HC = Char->FindComponentByClass<UHealthComponent>();
            if (HC && HC->IsDead()) continue;
            ValidPlayers.Add(Actor);
        }
    }

    if (ValidPlayers.Num() == 0) return;

    int32 RandomIndex = FMath::RandRange(0, ValidPlayers.Num() - 1);
    AActor* TargetPlayer = ValidPlayers[RandomIndex];

    if (TargetPlayer && SpikeTrapClass)
    {
        FVector TargetLoc = TargetPlayer->GetActorLocation();
        FVector SpawnLocation = TargetLoc;
        FRotator SpawnRotation = FRotator::ZeroRotator;

        FHitResult HitResult;
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(TargetPlayer);
        Params.AddIgnoredActor(this);

        bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, TargetLoc, TargetLoc - FVector(0, 0, 500.0f), ECC_WorldStatic, Params);
        if (bHit) SpawnLocation = HitResult.Location;
        else SpawnLocation.Z -= 90.0f;

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.Instigator = this;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        GetWorld()->SpawnActor<ABossSpikeTrap>(SpikeTrapClass, SpawnLocation, SpawnRotation, SpawnParams);
    }
}

// ============================================================================
// [Death Wall Sequence] - 최종 수정 (안전장치 + State Lock 강화)
// ============================================================================

void AStage1Boss::OnHealthChanged(float CurrentHealth, float MaxHealth)
{
    if (!HasAuthority()) return;
    if (bHasTriggeredDeathWall || CurrentHealth <= 0.0f) return;

    float HpRatio = (MaxHealth > 0.0f) ? (CurrentHealth / MaxHealth) : 0.0f;

    // [조건] HP 70% 이하가 되면 1회만 발동
    if (HpRatio <= 0.7f)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Stage1Boss] HP 70%% Reached! Starting Death Wall Sequence."));
        bHasTriggeredDeathWall = true; // 중복 실행 방지
        StartDeathWallSequence();
    }
}

void AStage1Boss::StartDeathWallSequence()
{
    AAIController* AIC = Cast<AAIController>(GetController());

    if (AIC && DeathWallCastLocation)
    {
        UE_LOG(LogTemp, Warning, TEXT(">>> [BOSS] Death Wall Sequence STARTED (Invincible ON)"));

        bIsDeathWallSequenceActive = true; // 무적 상태 ON

        // 1. 기존 동작 및 AI 강제 정지
        StopAnimMontage();
        if (UBrainComponent* Brain = AIC->GetBrainComponent())
        {
            Brain->StopLogic(TEXT("DeathWallPattern"));
        }
        AIC->StopMovement();
        AIC->ClearFocus(EAIFocusPriority::Gameplay); // 시선 고정 해제

        // 2. 이동 명령
        AIC->ReceiveMoveCompleted.AddDynamic(this, &AStage1Boss::OnArrivedAtCastLocation);
        AIC->MoveToActor(DeathWallCastLocation, 50.0f);

        // 3. ★ [Safety] 안전장치 타이머 시작 (15초 후 강제 종료)
        // 이동이 꼬이거나 애니메이션 노티파이가 씹혀서 영원히 무적이 되는 것을 방지
        GetWorldTimerManager().SetTimer(
            FailSafeTimerHandle,
            this,
            &AStage1Boss::FinishDeathWallPattern,
            DeathWallPatternDuration + 15.0f, // 넉넉하게 시간 부여
            false
        );
    }
    else
    {
        // 타겟 포인트가 없으면 즉시 제자리 시전
        UE_LOG(LogTemp, Warning, TEXT(">>> [BOSS] No Cast Location! Casting Immediately."));

        bIsDeathWallSequenceActive = true;
        StopAnimMontage();
        if (AIC)
        {
            AIC->StopMovement();
            AIC->ClearFocus(EAIFocusPriority::Gameplay);
            if (UBrainComponent* Brain = AIC->GetBrainComponent()) Brain->StopLogic(TEXT("DeathWallPattern"));
        }

        // 즉시 도착 처리
        OnArrivedAtCastLocation(FAIRequestID::CurrentRequest, EPathFollowingResult::Success);

        // 안전장치 타이머 가동
        GetWorldTimerManager().SetTimer(FailSafeTimerHandle, this, &AStage1Boss::FinishDeathWallPattern, DeathWallPatternDuration + 5.0f, false);
    }
}

void AStage1Boss::OnArrivedAtCastLocation(FAIRequestID RequestID, EPathFollowingResult::Type Result)
{
    if (Result == EPathFollowingResult::Success)
    {
        if (AAIController* AIC = Cast<AAIController>(GetController()))
        {
            // 델리게이트 해제
            AIC->ReceiveMoveCompleted.RemoveDynamic(this, &AStage1Boss::OnArrivedAtCastLocation);

            // [방향 정렬] 타겟 포인트 방향(벽이 나아갈 방향)으로 회전
            if (DeathWallCastLocation)
            {
                SetActorRotation(DeathWallCastLocation->GetActorRotation());
            }

            // 움직임 및 시선 확실히 고정
            AIC->StopMovement();
            AIC->ClearFocus(EAIFocusPriority::Gameplay);
        }

        UE_LOG(LogTemp, Log, TEXT("[Stage1Boss] Arrived! Playing Montage."));
        Multicast_PlayDeathWallMontage();
    }
}

void AStage1Boss::Multicast_PlayDeathWallMontage_Implementation()
{
    if (BossData && BossData->DeathWallSummonMontage)
    {
        PlayAnimMontage(BossData->DeathWallSummonMontage);
    }
    else
    {
        // 몽타주 없으면 수동 호출 (테스트용)
        if (HasAuthority())
        {
            AnimNotify_ActivateDeathWall();
        }
    }
}

void AStage1Boss::AnimNotify_ActivateDeathWall()
{
    if (!HasAuthority()) return;

    UE_LOG(LogTemp, Warning, TEXT(">>> [BOSS] AnimNotify Triggered: Spawning Wall!"));

    // 1. 벽 스폰
    SpawnDeathWall();

    // 2. ★ 타이머 재설정 (안전장치 해제 후 정규 대기 시간 설정)
    // 애니메이션이 정상 실행되었으므로, 불필요하게 긴 안전장치 타이머를 지우고
    // 정확한 대기 시간(DeathWallPatternDuration)으로 교체합니다.
    GetWorldTimerManager().ClearTimer(FailSafeTimerHandle);

    GetWorldTimerManager().SetTimer(
        DeathWallTimerHandle,
        this,
        &AStage1Boss::FinishDeathWallPattern,
        DeathWallPatternDuration, // 설정한 대기 시간 (예: 10초)
        false
    );
}

void AStage1Boss::FinishDeathWallPattern()
{
    if (!HasAuthority()) return;

    UE_LOG(LogTemp, Warning, TEXT(">>> [BOSS] Wall Pattern Finished. Invincible OFF. AI Resumed."));

    // 1. 상태 잠금 해제 (이제 다시 데미지 입음)
    bIsDeathWallSequenceActive = false;

    // 2. 잔여 타이머 모두 제거
    GetWorldTimerManager().ClearTimer(DeathWallTimerHandle);
    GetWorldTimerManager().ClearTimer(FailSafeTimerHandle);

    // 3. AI 재개
    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        if (UBrainComponent* Brain = AIC->GetBrainComponent())
        {
            Brain->RestartLogic(); // 뇌 재가동
        }
    }
}