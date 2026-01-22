// Source/Project_Bang_Squad/Character/StageBoss/Stage1Boss.cpp
#include "Stage1Boss.h"
#include "JobCrystal.h"
#include "DeathWall.h"
#include "Project_Bang_Squad/Projectile/SlashProjectile.h"
#include "Project_Bang_Squad/Core/TrueDamageType.h"
#include "Project_Bang_Squad/Core/BSGameInstance.h"
#include "Project_Bang_Squad/Game/Stage/StageGameMode.h"
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"

#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h"

// ============================================================================
// [Constructor & BeginPlay]
// ============================================================================

AStage1Boss::AStage1Boss()
{
    // 필요 시 캡슐 크기 조정
    // GetCapsuleComponent()->SetCapsuleSize(60.f, 120.f);
}

void AStage1Boss::BeginPlay()
{
    Super::BeginPlay();

    // [Check] 데이터 에셋 연결 확인
    if (HasAuthority() && !BossData)
    {
        UE_LOG(LogTemp, Error, TEXT("[AStage1Boss] BossData is MISSING! Please assign DA_StageBoss in Blueprint."));
    }

    if (HasAuthority())
    {
        // [정상 로직] 시작 시 기믹 페이즈(무적)로 진입
        SetPhase(EBossPhase::Gimmick);

        // [TEST용] 데미지 로직만 빠르게 확인할 때는 아래 주석을 풀고 위를 주석 처리하세요.
        // SetPhase(EBossPhase::Phase1); 
        // UE_LOG(LogTemp, Warning, TEXT("[TEST] Stage1Boss: Force Started in Phase 1 (Combat)"));
    }
}

// ============================================================================
// [Phase & Damage Logic]
// ============================================================================

float AStage1Boss::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    // 1. [서버 권한]
    if (!HasAuthority()) return 0.0f;

    // 2. [팀킬 방지]
    AActor* Attacker = DamageCauser;
    if (EventInstigator && EventInstigator->GetPawn())
    {
        Attacker = EventInstigator->GetPawn();
    }
    else if (DamageCauser && DamageCauser->GetOwner())
    {
        Attacker = DamageCauser->GetOwner();
    }

    if (Attacker)
    {
        if (Attacker == this) return 0.0f;
        if (Attacker->IsA(AEnemyCharacterBase::StaticClass())) return 0.0f;
    }

    // 3. 부모 로직 (무적 상태면 여기서 0 리턴됨)
    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    // [최적화] 데미지가 없으면 이후 로직 생략
    if (ActualDamage <= 0.0f) return 0.0f;

    // 4. [HealthComponent 적용]
    UHealthComponent* HC = FindComponentByClass<UHealthComponent>();
    if (HC && !HC->IsDead())
    {
        HC->ApplyDamage(ActualDamage);
    }
    else
    {
        return 0.0f;
    }

    // 5. [페이즈 전환 체크] (체력 절반 이하 시 2페이즈)
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

    // 1. 페이즈 전환 (2페이즈)
    SetPhase(EBossPhase::Phase2);

    // 2. 수정 재소환 및 무적 활성화 (플레이어가 다시 깨야 함)
    bIsInvincible = true;
    SpawnCrystals();

    // 3. 죽음의 벽 소환
    SpawnDeathWall();

    UE_LOG(LogTemp, Warning, TEXT("=== ENTERING PHASE 2: Crystals Respawned & Death Wall Activated! ==="));
}

void AStage1Boss::OnDeathStarted()
{
    Super::OnDeathStarted();

    if (!HasAuthority()) return;

    if (BossData && BossData->DeathMontage)
    {
        Multicast_PlayAttackMontage(BossData->DeathMontage);
    }

    UE_LOG(LogTemp, Warning, TEXT("=== BOSS DEFEATED! STAGE CLEAR! ==="));

    // 벽 정지
    TArray<AActor*> Walls;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADeathWall::StaticClass(), Walls);
    for (AActor* Wall : Walls)
    {
        Wall->SetActorTickEnabled(false);
    }
}

// ============================================================================
// [Gimmick Logic: Crystal]
// ============================================================================

void AStage1Boss::OnPhaseChanged(EBossPhase NewPhase)
{
    Super::OnPhaseChanged(NewPhase);

    // 시작 페이즈(Gimmick)가 되면 수정 소환
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

    // 소환할 직업 순서 (SpawnPoints 순서와 매칭됨)
    TArray<EJobType> JobOrder = { EJobType::Titan, EJobType::Striker, EJobType::Mage, EJobType::Paladin };
    RemainingGimmickCount = 0;

    for (int32 i = 0; i < CrystalSpawnPoints.Num(); ++i)
    {
        if (i >= JobOrder.Num()) break;

        EJobType CurrentJob = JobOrder[i]; // 이번 자리에 소환할 직업

        // [중요] TMap에서 해당 직업에 맞는 블루프린트를 찾음
        if (!JobCrystalClasses.Contains(CurrentJob))
        {
            UE_LOG(LogTemp, Error, TEXT("[Stage1Boss] Missing Crystal BP for Job: %d. Check Blueprint Settings!"), (int32)CurrentJob);
            continue;
        }

        TSubclassOf<AJobCrystal> TargetClass = JobCrystalClasses[CurrentJob];
        if (!TargetClass) continue;

        // 위치 계산 (보스 기준 로컬 좌표 -> 월드 좌표)
        FVector SpawnLoc = GetActorTransform().TransformPosition(CrystalSpawnPoints[i]);
        FRotator SpawnRot = FRotator::ZeroRotator;

        FActorSpawnParameters Params;
        Params.Owner = this;

        // 찾은 BP로 소환
        AJobCrystal* NewCrystal = GetWorld()->SpawnActor<AJobCrystal>(TargetClass, SpawnLoc, SpawnRot, Params);
        if (NewCrystal)
        {
            NewCrystal->TargetBoss = this;
            NewCrystal->RequiredJobType = CurrentJob; // 직업 정보 주입
            RemainingGimmickCount++;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Stage1Boss: Spawned %d Crystals."), RemainingGimmickCount);
}

void AStage1Boss::OnGimmickResolved(int32 GimmickID)
{
    if (!HasAuthority()) return;

    RemainingGimmickCount--;

    // 모든 수정 파괴 시 무적 해제
    if (RemainingGimmickCount <= 0)
    {
        if (bPhase2Started)
        {
            SetPhase(EBossPhase::Phase2);
            bIsInvincible = false;
        }
        else
        {
            SetPhase(EBossPhase::Phase1); // -> 무적 해제 (OnPhaseChanged 호출됨)
        }
        UE_LOG(LogTemp, Warning, TEXT("Stage1Boss: All Crystals Destroyed! Invincibility OFF!"));
    }
}

// ============================================================================
// [Combat Logic]
// ============================================================================

void AStage1Boss::DoAttack_Slash()
{
    if (!HasAuthority()) return;

    if (BossData && BossData->SlashAttackMontage)
    {
        Multicast_PlayAttackMontage(BossData->SlashAttackMontage, FName("Slash"));
    }
}

void AStage1Boss::DoAttack_Swing()
{
    if (!HasAuthority()) return;

    if (BossData && BossData->AttackMontages.Num() > 0)
    {
        Multicast_PlayAttackMontage(BossData->AttackMontages[0]);
    }
}

void AStage1Boss::Multicast_PlayAttackMontage_Implementation(UAnimMontage* MontageToPlay, FName SectionName)
{
    if (MontageToPlay)
    {
        PlayAnimMontage(MontageToPlay, 1.0f, SectionName);
    }
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
    float Radius = MeleeAttackRadius;

    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(this);

    TArray<AActor*> OutActors;

    bool bHit = UKismetSystemLibrary::SphereOverlapActors(
        GetWorld(), TraceStart, Radius, ObjectTypes, nullptr, ActorsToIgnore, OutActors
    );

    for (AActor* HitActor : OutActors)
    {
        if (HitActor && HitActor != this)
        {
            UGameplayStatics::ApplyDamage(HitActor, MeleeDamageAmount, GetController(), this, UDamageType::StaticClass());
        }
    }
}

// ============================================================================
// [QTE Logic]
// ============================================================================

void AStage1Boss::StartSpearQTE()
{
    if (!HasAuthority()) return;

    QTEProgressMap.Empty();

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (APlayerController* PC = It->Get())
        {
            if (PC->GetPawn())
            {
                QTEProgressMap.Add(PC, FPlayerQTEStatus());
            }
        }
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

    if (Status.PressCount > QTEGoalCount)
    {
        Status.bFailed = true;
    }
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
        if (bVisible)
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("[UI] PRESS SPACE 10 TIMES!!!"));
        else
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("[UI] QTE END"));
    }
}

void AStage1Boss::SpawnDeathWall()
{
    if (!HasAuthority() || !DeathWallClass) return;

    FActorSpawnParameters Params;
    Params.Owner = this;
    FRotator SpawnRot = GetActorRotation();

    ADeathWall* NewWall = GetWorld()->SpawnActor<ADeathWall>(DeathWallClass, WallSpawnLocation, SpawnRot, Params);
    if (NewWall)
    {
        NewWall->ActivateWall();
    }
}