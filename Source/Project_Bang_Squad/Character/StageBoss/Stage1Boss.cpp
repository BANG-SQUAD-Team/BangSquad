// Source/Project_Bang_Squad/Character/StageBoss/Stage1Boss.cpp

#include "Stage1Boss.h"
#include "JobCrystal.h"
#include "DeathWall.h"
#include "Project_Bang_Squad/Projectile/SlashProjectile.h"
#include "Project_Bang_Squad/Core/TrueDamageType.h"
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h" // [추가] 무브먼트 모드 설정을 위해 필수
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"
#include "Engine/TargetPoint.h"
#include "BossSpikeTrap.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Net/UnrealNetwork.h"
#include "Project_Bang_Squad/BossPattern/Boss1_Rampart.h"

AStage1Boss::AStage1Boss() {}

void AStage1Boss::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority())
    {
        SetPhase(EBossPhase::Gimmick);
        if (UHealthComponent* HC = FindComponentByClass<UHealthComponent>())
            HC->OnHealthChanged.AddDynamic(this, &AStage1Boss::OnHealthChanged);
    }
}

float AStage1Boss::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    // [무적 처리] 패턴 진행 중(bIsDeathWallSequenceActive)일 때 데미지 0 처리
    if (!HasAuthority() || bIsDeathWallSequenceActive) return 0.0f;

    AActor* Attacker = DamageCauser;
    if (EventInstigator && EventInstigator->GetPawn()) Attacker = EventInstigator->GetPawn();
    if (Attacker && (Attacker == this || Attacker->IsA(AEnemyCharacterBase::StaticClass()))) return 0.0f;

    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    if (ActualDamage <= 0.0f) return 0.0f;

    if (UHealthComponent* HC = FindComponentByClass<UHealthComponent>())
    {
        HC->ApplyDamage(ActualDamage);
        if (HC->GetHealth() / HC->MaxHealth <= (BossData ? BossData->GimmickThresholdRatio : 0.5f) && !bPhase2Started && CurrentPhase == EBossPhase::Phase1)
            EnterPhase2();
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
}

void AStage1Boss::OnDeathStarted()
{
    Super::OnDeathStarted();
    if (!HasAuthority()) return;
    if (BossData && BossData->DeathMontage) Multicast_PlayAttackMontage(BossData->DeathMontage);
    TArray<AActor*> Walls; UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADeathWall::StaticClass(), Walls);
    for (AActor* W : Walls) W->SetActorTickEnabled(false);
}

void AStage1Boss::OnPhaseChanged(EBossPhase NewPhase)
{
    Super::OnPhaseChanged(NewPhase);
    if (NewPhase == EBossPhase::Gimmick) { bIsInvincible = true; SpawnCrystals(); }
    else if (NewPhase == EBossPhase::Phase1) bIsInvincible = false;
}

void AStage1Boss::SpawnCrystals()
{
    if (!HasAuthority() || CrystalSpawnPoints.Num() == 0) return;
    TArray<EJobType> JobOrder = { EJobType::Titan, EJobType::Striker, EJobType::Mage, EJobType::Paladin };
    RemainingGimmickCount = 0;
    for (int32 i = 0; i < CrystalSpawnPoints.Num(); ++i)
    {
        if (i >= JobOrder.Num()) break;
        if (!JobCrystalClasses.Contains(JobOrder[i]) || !CrystalSpawnPoints[i]) continue;
        if (AJobCrystal* NC = GetWorld()->SpawnActor<AJobCrystal>(JobCrystalClasses[JobOrder[i]], CrystalSpawnPoints[i]->GetActorLocation(), FRotator::ZeroRotator))
        {
            NC->TargetBoss = this; NC->RequiredJobType = JobOrder[i]; RemainingGimmickCount++;
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

// --- [중요: 행동 잠금 및 몽타주 로직] ---

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
    if (HasAuthority())
    {
        // DeathWall 패턴 중일 때는 몽타주가 끝나도 잠금을 풀지 않음 (타이머가 풀 때까지 대기)
        if (!bIsDeathWallSequenceActive)
        {
            bIsActionInProgress = false;
            UE_LOG(LogTemp, Log, TEXT("[Stage1Boss] Action Lock Released."));
        }
    }
}

void AStage1Boss::AnimNotify_SpawnSlash()
{
    if (!HasAuthority() || !BossData || !BossData->SlashProjectileClass) return;
    GetWorld()->SpawnActor<ASlashProjectile>(BossData->SlashProjectileClass, GetActorLocation() + GetActorForwardVector() * 150.f, GetActorRotation());
}

void AStage1Boss::AnimNotify_CheckMeleeHit()
{
    if (!HasAuthority()) return;
    TArray<TEnumAsByte<EObjectTypeQuery>> O; O.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
    TArray<AActor*> I; I.Add(this); TArray<AActor*> Out;
    if (UKismetSystemLibrary::SphereOverlapActors(GetWorld(), GetActorLocation(), MeleeAttackRadius, O, nullptr, I, Out))
        for (AActor* A : Out) UGameplayStatics::ApplyDamage(A, MeleeDamageAmount, GetController(), this, UDamageType::StaticClass());
}

// --- [QTE & DeathWall & Spike] ---

void AStage1Boss::StartSpearQTE()
{
    if (!HasAuthority()) return;
    QTEProgressMap.Empty();
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (APlayerController* PC = It->Get()) if (PC->GetPawn()) QTEProgressMap.Add(PC, FPlayerQTEStatus());
    if (QTEProgressMap.Num() == 0) return;
    Multicast_SetQTEWidget(true);
    GetWorldTimerManager().SetTimer(QTETimerHandle, this, &AStage1Boss::EndSpearQTE, QTEDuration, false);
}

void AStage1Boss::Server_SubmitQTEInput_Implementation(APlayerController* PC)
{
    if (!GetWorldTimerManager().IsTimerActive(QTETimerHandle) || !QTEProgressMap.Contains(PC)) return;
    if (++QTEProgressMap[PC].PressCount > QTEGoalCount) QTEProgressMap[PC].bFailed = true;
}

void AStage1Boss::EndSpearQTE()
{
    if (!HasAuthority()) return;
    Multicast_SetQTEWidget(false);
    bool S = true;
    for (auto& P : QTEProgressMap) if (P.Value.PressCount != QTEGoalCount || P.Value.bFailed) { S = false; break; }
    if (S) { if (!bPhase2Started) EnterPhase2(); else SpawnDeathWall(); }
    else PerformWipeAttack();
}

void AStage1Boss::PerformWipeAttack()
{
    for (auto& P : QTEProgressMap) if (P.Key && P.Key->GetPawn())
        UGameplayStatics::ApplyDamage(P.Key->GetPawn(), 1000.f, GetController(), this, UTrueDamageType::StaticClass());
}

void AStage1Boss::Multicast_SetQTEWidget_Implementation(bool V) { if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, V ? FColor::Red : FColor::Green, V ? TEXT("QTE!") : TEXT("END")); }

void AStage1Boss::OnHealthChanged(float CH, float MH)
{
    if (HasAuthority() && !bHasTriggeredDeathWall && CH > 0 && (CH / MH) <= 0.7f)
    {
        bHasTriggeredDeathWall = true; StartDeathWallSequence();
    }
}

// --------------------------------------------------------
// [수정됨] 패턴 시작: 성벽 내리기 + 타이머 설정
// --------------------------------------------------------
void AStage1Boss::StartDeathWallSequence()
{
    if (!HasAuthority()) return;

    UE_LOG(LogTemp, Warning, TEXT(">>> [BOSS] Death Wall Sequence Started. Ramparts Sinking."));

    // 1. [추가] 맵에 있는 모든 성벽을 찾아서 내립니다 (Sinking)
    ControlRamparts(true);

    // 2. [추가] 1분 45초(105초) 뒤에 성벽을 다시 올리는 타이머 설정
    GetWorldTimerManager().SetTimer(
        RampartTimerHandle,
        this,
        &AStage1Boss::RestoreRamparts,
        105.0f, // 1분 45초
        false
    );

    // --- 기존 로직 유지 ---
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

void AStage1Boss::OnArrivedAtCastLocation(FAIRequestID RID, EPathFollowingResult::Type R)
{
    if (R == EPathFollowingResult::Success)
    {
        if (AAIController* AIC = Cast<AAIController>(GetController()))
        {
            AIC->ReceiveMoveCompleted.RemoveDynamic(this, &AStage1Boss::OnArrivedAtCastLocation); AIC->StopMovement();
        }
        if (DeathWallCastLocation) SetActorRotation(DeathWallCastLocation->GetActorRotation());
        Multicast_PlayDeathWallMontage();
    }
}

void AStage1Boss::Multicast_PlayDeathWallMontage_Implementation()
{
    UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
    if (AnimInstance && BossData && BossData->DeathWallSummonMontage)
    {
        if (AnimInstance->Montage_IsPlaying(BossData->DeathWallSummonMontage)) return;
        PlayAnimMontage(BossData->DeathWallSummonMontage);
    }
    else if (HasAuthority())
    {
        AnimNotify_ActivateDeathWall();
    }
}

// --------------------------------------------------------
// [수정됨] 벽 스폰 트리거: 보스는 60초 뒤에 풀려남
// --------------------------------------------------------
void AStage1Boss::AnimNotify_ActivateDeathWall()
{
    if (!HasAuthority()) return;

    // 1. 벽 스폰 (벽의 수명은 SpawnDeathWall 내부에서 설정)
    SpawnDeathWall();

    // 2. 기존 안전장치 타이머 제거
    GetWorldTimerManager().ClearTimer(FailSafeTimerHandle);

    // 3. [핵심] 보스 행동 재개 타이머 (60초)
    // 벽은 1분 40초(100초) 살지만, 보스는 1분(60초) 뒤에 먼저 움직입니다.
    GetWorldTimerManager().SetTimer(
        DeathWallTimerHandle,
        this,
        &AStage1Boss::FinishDeathWallPattern,
        60.0f,
        false
    );
}

// --------------------------------------------------------
// [수정됨] 벽 스폰 로직: 100초 수명 설정 + 충돌 무시
// --------------------------------------------------------
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
        // [핵심] 벽 수명 설정: 1분 40초 (100초)
        // DeathWall.cpp를 수정하지 않아도 여기서 강제 설정합니다.
        NewWall->SetLifeSpan(110.0f);

        // [핵심] 상호 충돌 무시 (영구적)
        // 보스 -> 벽 무시
        if (GetCapsuleComponent())
        {
            GetCapsuleComponent()->IgnoreActorWhenMoving(NewWall, true);
        }
        // 벽 -> 보스 무시
        if (UPrimitiveComponent* WallRoot = Cast<UPrimitiveComponent>(NewWall->GetRootComponent()))
        {
            WallRoot->IgnoreActorWhenMoving(this, true);
        }

        UE_LOG(LogTemp, Warning, TEXT(">>> [BOSS] Wall Spawned. Life: 100s. Boss Waiting: 60s."));

        NewWall->ActivateWall();
    }
}

// --------------------------------------------------------
// [수정됨] 60초 뒤 보스 정상화 (벽은 아직 40초 남음)
// --------------------------------------------------------
// --------------------------------------------------------
// [수정됨] 패턴 종료 함수 (기존 로직 + 타이머 정리)
// --------------------------------------------------------
void AStage1Boss::FinishDeathWallPattern()
{
    if (!HasAuthority()) return;

    // ... 기존 로직 (무브먼트 복구, AI 재시작 등) ...
    if (GetCharacterMovement()) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    bIsDeathWallSequenceActive = false;
    bIsActionInProgress = false;
    bIsInvincible = false;

    // 기존 타이머들 정리
    GetWorldTimerManager().ClearTimer(DeathWallTimerHandle);
    GetWorldTimerManager().ClearTimer(FailSafeTimerHandle);

    // 주의: RampartTimer는 105초라 아직 돌고 있을 수 있으니 여기서 굳이 끄지 않습니다.
    // 만약 "보스 패턴이 강제 종료되면 성벽도 즉시 올라와야 한다"면 여기서 RestoreRamparts()를 호출하세요.

    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        if (UBrainComponent* BC = AIC->GetBrainComponent()) BC->RestartLogic();
    }

    UE_LOG(LogTemp, Warning, TEXT(">>> [BOSS] Boss Active Again."));
}

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
    for (AActor* A : Found) { ACharacter* C = Cast<ACharacter>(A); if (C && A != this && C->IsPlayerControlled()) Valid.Add(A); }
    if (Valid.Num() == 0) return;
    AActor* T = Valid[FMath::RandRange(0, Valid.Num() - 1)];
    if (T && SpikeTrapClass)
    {
        FVector L = T->GetActorLocation(); FHitResult H;
        L = GetWorld()->LineTraceSingleByChannel(H, L, L - FVector(0, 0, 500), ECC_WorldStatic) ? H.Location : L - FVector(0, 0, 90);
        GetWorld()->SpawnActor<ABossSpikeTrap>(SpikeTrapClass, L, FRotator::ZeroRotator);
    }
}

void AStage1Boss::TrySpawnSpikeAtRandomPlayer() { if (HasAuthority()) { if (SpellMontage) Multicast_PlaySpellMontage(); ExecuteSpikeSpell(); } }
void AStage1Boss::Multicast_PlaySpellMontage_Implementation() { if (GetMesh() && GetMesh()->GetAnimInstance() && SpellMontage) GetMesh()->GetAnimInstance()->Montage_Play(SpellMontage); }

// --------------------------------------------------------
// [추가됨] 성벽 제어 헬퍼 함수
// --------------------------------------------------------
void AStage1Boss::ControlRamparts(bool bSink)
{
    if (!HasAuthority()) return;

    TArray<AActor*> FoundRamparts;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABoss1_Rampart::StaticClass(), FoundRamparts);

    for (AActor* Actor : FoundRamparts)
    {
        if (ABoss1_Rampart* Rampart = Cast<ABoss1_Rampart>(Actor))
        {
            // true면 내리고(Sink), false면 올림(Rise)
            Rampart->Server_SetRampartActive(bSink);
        }
    }
}

// --------------------------------------------------------
// [추가됨] 105초 뒤 호출: 성벽 복구
// --------------------------------------------------------
void AStage1Boss::RestoreRamparts()
{
    if (!HasAuthority()) return;

    // 성벽을 다시 올립니다 (Rising)
    ControlRamparts(false);

    // 타이머 정리
    GetWorldTimerManager().ClearTimer(RampartTimerHandle);

    UE_LOG(LogTemp, Warning, TEXT(">>> [BOSS] 1m 45s Passed. Ramparts Rising."));
}