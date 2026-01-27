// Source/Project_Bang_Squad/Character/StageBoss/Stage1Boss.cpp

#include "Stage1Boss.h"
#include "JobCrystal.h"
#include "DeathWall.h"
#include "BossSpikeTrap.h"
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

        // [수정됨] 이름 충돌 방지: MeleeCollisionBox 변수에 블루프린트의 BoxComponent를 찾아 연결
        if (!IsValid(MeleeCollisionBox))
        {
            MeleeCollisionBox = Cast<UBoxComponent>(GetComponentByClass(UBoxComponent::StaticClass()));
        }
    }
}

// ==============================================================================
// [1] 데미지 처리 및 페이즈 전환
// ==============================================================================

float AStage1Boss::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    // [무적 처리] 패턴 진행 중일 때 데미지 0
    if (!HasAuthority() || bIsDeathWallSequenceActive) return 0.0f;

    // 팀킬 방지
    AActor* Attacker = DamageCauser;
    if (EventInstigator && EventInstigator->GetPawn()) Attacker = EventInstigator->GetPawn();
    if (Attacker && (Attacker == this || Attacker->IsA(AEnemyCharacterBase::StaticClass()))) return 0.0f;

    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    if (ActualDamage <= 0.0f) return 0.0f;

    // 페이즈 전환 체크
    if (UHealthComponent* HC = FindComponentByClass<UHealthComponent>())
    {
        HC->ApplyDamage(ActualDamage);
        float HpRatio = HC->GetHealth() / HC->MaxHealth;

        // 체력 조건 만족 시 페이즈 2 진입
        if (HpRatio <= (BossData ? BossData->GimmickThresholdRatio : 0.5f) && !bPhase2Started && CurrentPhase == EBossPhase::Phase1)
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
    SpawnCrystals(); // 2페이즈 시작 시 수정 소환
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

// 1. 3초간 멈추기 (꼼수 시작)
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

    // [핵심] 0.0f가 아니라 0.01f로 설정합니다.
    // 아주 미세하게 흐르게 해서 엔진이 애니메이션을 죽이지 않게 만듭니다.
    Multicast_SetAttackPlayRate(0.01f);

    UE_LOG(LogTemp, Error, TEXT(">>> BOSS HOLD STARTED (Speed: 0.01) <<<"));

    // 3초 뒤 재개
    GetWorldTimerManager().SetTimer(MeleeAttackTimerHandle, this, &AStage1Boss::ReleaseMeleeAttackHold, 3.0f, false);
}

// 2. 3초 뒤 재개
void AStage1Boss::ReleaseMeleeAttackHold()
{
    if (!HasAuthority()) return;

    UE_LOG(LogTemp, Error, TEXT(">>> BOSS RELEASED (Speed: 1.0) <<<"));

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

// 3. 멀티캐스트 (블렌드 아웃 방지 + 속도 조절)
void AStage1Boss::Multicast_SetAttackPlayRate_Implementation(float NewRate)
{
    UAnimInstance* AnimInst = GetMesh()->GetAnimInstance();
    if (!AnimInst) return;

    UAnimMontage* CurrentMontage = AnimInst->GetCurrentActiveMontage();
    if (CurrentMontage)
    {
        // 1. 속도 적용
        AnimInst->Montage_SetPlayRate(CurrentMontage, NewRate);

        // 2. 몽타주 인스턴스 가져오기
        if (FAnimMontageInstance* MontageInst = AnimInst->GetActiveInstanceForMontage(CurrentMontage))
        {
            // 속도가 1보다 작을 때(멈출 때)는 절대 끄지 말라고 협박
            if (NewRate < 0.5f)
            {
                MontageInst->bEnableAutoBlendOut = false;
            }
            // 다시 움직일 때는 정상적으로 끝나게 허용
            else
            {
                MontageInst->bEnableAutoBlendOut = true;
            }
        }
    }
    else
    {
        // 만약 몽타주가 이미 꺼졌다면? 강제로 다시 재생 시도 (심폐소생술)
        if (NewRate > 0.5f && BossData && BossData->AttackMontages.Num() > 0)
        {
            // 마지막 수단: 공격이 끊겼으면 처음부터 다시라도 때려라
            // PlayAnimMontage(BossData->AttackMontages[0]); 
            // (이 부분은 동작이 튀므로 일단 주석 처리, 위 0.01f 로직으로 해결될 것입니다)
        }
    }
}

// [노티파이 2] 실제 타격 지점 (박스 콜리전 판정)
void AStage1Boss::AnimNotify_ExecuteMeleeHit()
{
    if (!HasAuthority()) return; // 데미지 판정은 서버 권한

    // 1. 박스 컴포넌트 찾기 (없으면 찾음)
    if (!IsValid(MeleeCollisionBox))
    {
        MeleeCollisionBox = Cast<UBoxComponent>(GetComponentByClass(UBoxComponent::StaticClass()));
        if (!MeleeCollisionBox)
        {
            UE_LOG(LogTemp, Error, TEXT("[Stage1Boss] MeleeCollisionBox Missing!"));
            return;
        }
    }

    // 2. 박스 안에 있는 액터들 가져오기
    TArray<AActor*> OverlappingActors;
    MeleeCollisionBox->GetOverlappingActors(OverlappingActors, ACharacter::StaticClass());

    for (AActor* Target : OverlappingActors)
    {
        // 보스 자신은 제외
        if (Target && Target != this)
        {
            // 데미지 적용
            UGameplayStatics::ApplyDamage(
                Target,
                MeleeDamageAmount,
                GetController(),
                this,
                UDamageType::StaticClass()
            );

            UE_LOG(LogTemp, Warning, TEXT("[BOSS] Hit Target via Box: %s"), *Target->GetName());
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

    UE_LOG(LogTemp, Warning, TEXT(">>> [BOSS] Death Wall Sequence Started. Ramparts Sinking."));

    // 1. 성벽 내리기
    ControlRamparts(true);

    // 2. 1분 45초(105초) 후 성벽 복구 예약
    GetWorldTimerManager().SetTimer(RampartTimerHandle, this, &AStage1Boss::RestoreRamparts, 105.0f, false);

    // 3. 보스 상태 설정
    if (GetCharacterMovement()) GetCharacterMovement()->SetMovementMode(MOVE_None);
    bIsDeathWallSequenceActive = true;
    bIsActionInProgress = true;
    bIsInvincible = true;

    // 4. 안전한 위치로 이동 (캐스팅 장소)
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

    // 1. 벽 스폰
    SpawnDeathWall();

    // 2. 보스 행동 재개 타이머 (60초 뒤 보스는 움직임)
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
        // 벽 수명 110초 (보스보다 40~50초 더 오래 남음)
        NewWall->SetLifeSpan(110.0f);

        // 보스와 벽 서로 충돌 무시
        if (GetCapsuleComponent()) GetCapsuleComponent()->IgnoreActorWhenMoving(NewWall, true);
        if (UPrimitiveComponent* WallRoot = Cast<UPrimitiveComponent>(NewWall->GetRootComponent())) WallRoot->IgnoreActorWhenMoving(this, true);

        NewWall->ActivateWall();
    }
}

void AStage1Boss::FinishDeathWallPattern()
{
    if (!HasAuthority()) return;

    // 보스 정상화
    if (GetCharacterMovement()) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    bIsDeathWallSequenceActive = false;
    bIsActionInProgress = false;
    bIsInvincible = false;

    GetWorldTimerManager().ClearTimer(DeathWallTimerHandle);

    // AI 재시작
    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        if (UBrainComponent* BC = AIC->GetBrainComponent()) BC->RestartLogic();
    }

    UE_LOG(LogTemp, Warning, TEXT(">>> [BOSS] Boss Active Again."));
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
    ControlRamparts(false); // 다시 올리기
    GetWorldTimerManager().ClearTimer(RampartTimerHandle);
    UE_LOG(LogTemp, Warning, TEXT(">>> [BOSS] Ramparts Restored."));
}

void AStage1Boss::Multicast_PlayDeathWallMontage_Implementation()
{
    UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
    if (AnimInstance && BossData && BossData->DeathWallSummonMontage)
    {
        if (!AnimInstance->Montage_IsPlaying(BossData->DeathWallSummonMontage))
            PlayAnimMontage(BossData->DeathWallSummonMontage);
    }
    // 안전장치: 애니메이션이 없으면 바로 실행
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
// [5] QTE 및 기타 스킬
// ==============================================================================

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

void AStage1Boss::Server_SubmitQTEInput_Implementation(APlayerController* PC)
{
    if (!GetWorldTimerManager().IsTimerActive(QTETimerHandle) || !QTEProgressMap.Contains(PC)) return;
    if (++QTEProgressMap[PC].PressCount > QTEGoalCount) QTEProgressMap[PC].bFailed = true;
}

void AStage1Boss::EndSpearQTE()
{
    if (!HasAuthority()) return;
    Multicast_SetQTEWidget(false);
    bool bSuccess = true;
    for (auto& P : QTEProgressMap)
    {
        if (P.Value.PressCount != QTEGoalCount || P.Value.bFailed) { bSuccess = false; break; }
    }

    if (bSuccess) { if (!bPhase2Started) EnterPhase2(); else SpawnDeathWall(); }
    else PerformWipeAttack();
}

void AStage1Boss::PerformWipeAttack()
{
    for (auto& P : QTEProgressMap)
    {
        if (P.Key && P.Key->GetPawn())
            UGameplayStatics::ApplyDamage(P.Key->GetPawn(), 1000.f, GetController(), this, UTrueDamageType::StaticClass());
    }
}

void AStage1Boss::Multicast_SetQTEWidget_Implementation(bool V)
{
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, V ? FColor::Red : FColor::Green, V ? TEXT("QTE START!") : TEXT("QTE END"));
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