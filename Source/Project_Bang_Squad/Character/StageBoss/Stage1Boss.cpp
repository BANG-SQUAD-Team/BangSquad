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
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"
#include "Engine/TargetPoint.h"
#include "BossSpikeTrap.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Net/UnrealNetwork.h"

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

    // 왜 이렇게 짰는가: 서버에서만 AI 행동 잠금을 걸어 새로운 상태 전이를 막습니다.
    if (HasAuthority()) bIsActionInProgress = true;

    UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
    if (AnimInstance)
    {
        float Duration = AnimInstance->Montage_Play(MontageToPlay);
        if (SectionName != NAME_None) AnimInstance->Montage_JumpToSection(SectionName, MontageToPlay);

        // 왜 이렇게 짰는가: 몽타주가 끝날 때 반드시 서버에서 bIsActionInProgress를 풀어줘야 합니다.
        if (HasAuthority())
        {
            FOnMontageEnded EndDelegate;
            EndDelegate.BindUObject(this, &AStage1Boss::OnMontageEnded);
            AnimInstance->Montage_SetEndDelegate(EndDelegate, MontageToPlay);

            // 안전장치: 만약 몽타주 재생 시간이 0 이하라면 즉시 풀어줍니다.
            if (Duration <= 0.f) bIsActionInProgress = false;
        }
    }
}

void AStage1Boss::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    // 왜 이렇게 짰는가: 몽타주가 끝나거나, 다른 몽타주에 의해 중단(Interrupted)되어도 잠금을 풀어야 AI가 다음 행동을 합니다.
    if (HasAuthority())
    {
        bIsActionInProgress = false;
        UE_LOG(LogTemp, Log, TEXT("[Stage1Boss] Action Lock Released."));
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

void AStage1Boss::StartDeathWallSequence()
{
    if (!HasAuthority()) return;

    UE_LOG(LogTemp, Warning, TEXT(">>> [BOSS] Death Wall: Stationary Mode Start"));

    // 1. 상태 잠금 및 무적 설정
    bIsDeathWallSequenceActive = true;
    bIsActionInProgress = true;

    // 2. AI 판단만 일시정지 (이동 명령을 내리지 않으므로 안전함)
    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        AIC->StopMovement();
        if (UBrainComponent* BC = AIC->GetBrainComponent())
        {
            BC->PauseLogic(TEXT("DeathWallStationary"));
        }
    }

    // 3. 몽타주 재생 (제자리 점프 3번)
    Multicast_PlayDeathWallMontage();

    // 4. [핵심] 안전장치 타이머 - 애니메이션이 씹혀도 10~15초 뒤엔 무조건 무적 풀림
    GetWorldTimerManager().SetTimer(
        FailSafeTimerHandle,
        this,
        &AStage1Boss::FinishDeathWallPattern,
        15.0f,
        false
    );
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
    // 왜 이렇게 짰는가: 이미 해당 몽타주가 재생 중인지 확인하여 무한 루프(재귀 호출)를 방지합니다.
    UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
    if (AnimInstance && BossData && BossData->DeathWallSummonMontage)
    {
        if (AnimInstance->Montage_IsPlaying(BossData->DeathWallSummonMontage))
        {
            return; // 이미 재생 중이면 중복 실행 차단 (크래시 방지 핵심)
        }

        PlayAnimMontage(BossData->DeathWallSummonMontage);
    }
    else if (HasAuthority())
    {
        // 몽타주가 없는 비상 상황에서만 직접 호출
        AnimNotify_ActivateDeathWall();
    }
}

void AStage1Boss::AnimNotify_ActivateDeathWall()
{
    if (!HasAuthority()) return;
    SpawnDeathWall();
    GetWorldTimerManager().ClearTimer(FailSafeTimerHandle);
    GetWorldTimerManager().SetTimer(DeathWallTimerHandle, this, &AStage1Boss::FinishDeathWallPattern, DeathWallPatternDuration, false);
}

void AStage1Boss::SpawnDeathWall()
{
    if (!HasAuthority() || !DeathWallClass) return;

    FVector SpawnLoc;
    FRotator SpawnRot;

    // 보스 위치가 아닌, 레벨에 배치한 'DeathWallCastLocation' 기준 생성
    if (IsValid(DeathWallCastLocation))
    {
        SpawnLoc = DeathWallCastLocation->GetActorLocation();
        SpawnRot = DeathWallCastLocation->GetActorRotation();
    }
    else
    {
        // 타겟 포인트가 없으면 보스 앞 500유닛 지점에 생성 (방어 코드)
        SpawnLoc = GetActorLocation() + GetActorForwardVector() * 500.0f;
        SpawnRot = GetActorRotation();
        UE_LOG(LogTemp, Error, TEXT("DeathWallCastLocation is missing in Level!"));
    }

    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.Instigator = this;

    ADeathWall* NewWall = GetWorld()->SpawnActor<ADeathWall>(DeathWallClass, SpawnLoc, SpawnRot, Params);
    if (NewWall)
    {
        NewWall->ActivateWall();
    }
}

void AStage1Boss::FinishDeathWallPattern()
{
    if (!HasAuthority()) return;

    // 무적 및 행동 잠금 해제
    bIsDeathWallSequenceActive = false;
    bIsActionInProgress = false;

    GetWorldTimerManager().ClearTimer(DeathWallTimerHandle);
    GetWorldTimerManager().ClearTimer(FailSafeTimerHandle);

    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        if (UBrainComponent* BC = AIC->GetBrainComponent())
        {
            BC->RestartLogic(); // AI 재가동
        }
    }

    UE_LOG(LogTemp, Warning, TEXT(">>> [BOSS] Death Wall Pattern Ended. Invincible OFF."));
}


void AStage1Boss::StartSpikePattern()
{
    if (HasAuthority() && BossData && BossData->SpellMontage)
    {
        bIsActionInProgress = true; // 여기서 잠금!
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