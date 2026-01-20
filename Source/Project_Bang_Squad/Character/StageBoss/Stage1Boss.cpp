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

// ============================================================================
// [Constructor & BeginPlay]
// ============================================================================

AStage1Boss::AStage1Boss()
{
    // 보스 덩치에 맞게 캡슐 크기 조정
    // GetCapsuleComponent()->SetCapsuleSize(60.f, 120.f);
}

void AStage1Boss::BeginPlay()
{
    Super::BeginPlay();

    // [Check] 데이터 에셋이 연결되어 있는지 개발자에게 경고
    if (HasAuthority() && !BossData)
    {
        UE_LOG(LogTemp, Error, TEXT("[AStage1Boss] BossData is MISSING! Please assign DA_StageBoss in Blueprint."));
    }

    // 시작 시 기믹 페이즈(무적)로 시작
    if (HasAuthority())
    {
        SetPhase(EBossPhase::Gimmick);
    }
}

// ============================================================================
// [Phase & Damage Logic]
// ============================================================================

float AStage1Boss::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    // 1. 부모 로직 실행 (데미지 적용 및 무적 체크)
    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    if (!HasAuthority()) return ActualDamage;

    // 2. HealthComponent 가져오기
    UHealthComponent* HC = FindComponentByClass<UHealthComponent>();
    if (!HC) return ActualDamage;

    // 3. 체력 체크 (페이즈 2 진입)
    if (HC->MaxHealth > 0.0f)
    {
        float HpRatio = HC->GetHealth() / HC->MaxHealth;

        // [DataDriven] 데이터 에셋의 비율값 사용 (기본값 0.5)
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

    // 1. 페이즈 상태 변경
    SetPhase(EBossPhase::Phase2);

    // 2. 수정 재소환 + 무적
    bIsInvincible = true;
    SpawnCrystals();

    // 3. 죽음의 벽 소환 (압박 시작)
    SpawnDeathWall();

    UE_LOG(LogTemp, Warning, TEXT("=== ENTERING PHASE 2: Crystals Respawned & Death Wall Activated! ==="));
}

void AStage1Boss::OnDeathStarted()
{
    Super::OnDeathStarted();

    if (!HasAuthority()) return;

    // [DataDriven] 사망 몽타주 재생 (있다면)
    if (BossData && BossData->DeathMontage)
    {
        Multicast_PlayAttackMontage(BossData->DeathMontage);
    }

    UE_LOG(LogTemp, Warning, TEXT("=== BOSS DEFEATED! STAGE CLEAR! ==="));

    // 1. 벽 정지
    TArray<AActor*> Walls;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADeathWall::StaticClass(), Walls);
    for (AActor* Wall : Walls)
    {
        Wall->SetActorTickEnabled(false);
    }

    // 2. 게임 모드에 승리 알림 (예시)
    // if (AStageGameMode* GM = Cast<AStageGameMode>(GetWorld()->GetAuthGameMode()))
    // {
    //     GM->OnBossKilled();
    // }
}

// ============================================================================
// [Gimmick Logic: Crystal]
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
    if (!HasAuthority() || !CrystalClass) return;

    TArray<EJobType> JobOrder = { EJobType::Titan, EJobType::Striker, EJobType::Mage, EJobType::Paladin };
    RemainingGimmickCount = 0;

    for (int32 i = 0; i < CrystalSpawnPoints.Num(); ++i)
    {
        if (i >= JobOrder.Num()) break;

        FVector SpawnLoc = GetActorTransform().TransformPosition(CrystalSpawnPoints[i]);
        FRotator SpawnRot = FRotator::ZeroRotator;

        FActorSpawnParameters Params;
        Params.Owner = this;

        AJobCrystal* NewCrystal = GetWorld()->SpawnActor<AJobCrystal>(CrystalClass, SpawnLoc, SpawnRot, Params);
        if (NewCrystal)
        {
            NewCrystal->TargetBoss = this;
            NewCrystal->RequiredJobType = JobOrder[i];
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
        UE_LOG(LogTemp, Warning, TEXT("Stage1Boss: All Crystals Destroyed! Invincibility OFF!"));
    }
}

// ============================================================================
// [Combat Logic: Slash & Swing (DataDriven)]
// ============================================================================

void AStage1Boss::DoAttack_Slash()
{
    if (!HasAuthority()) return;

    if (BossData && BossData->SlashAttackMontage)
    {
        // [DataDriven] 참격 몽타주 실행
        Multicast_PlayAttackMontage(BossData->SlashAttackMontage, FName("Slash"));
        // 참고: 섹션 이름이 필요 없다면 NAME_None 혹은 생략 가능
    }
}

void AStage1Boss::DoAttack_Swing()
{
    if (!HasAuthority()) return;

    // [DataDriven] 공격 몽타주 배열 중 0번(예시) 실행
    // 데이터 에셋에 'AttackMontages' 배열이 있으므로 거기서 랜덤으로 꺼내거나 지정해서 쓸 수 있음
    if (BossData && BossData->AttackMontages.Num() > 0)
    {
        Multicast_PlayAttackMontage(BossData->AttackMontages[0]);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("No AttackMontages in BossData!"));
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
    // [DataDriven] 데이터 에셋에 정의된 투사체 클래스 사용
    if (!HasAuthority() || !BossData || !BossData->SlashProjectileClass) return;

    FVector SpawnLocation = GetActorLocation() + GetActorForwardVector() * 100.0f;
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
    // 반경 정보도 추후엔 DataAsset으로 이동 권장
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
            UGameplayStatics::ApplyDamage(
                HitActor,
                MeleeDamageAmount,
                GetController(),
                this,
                UDamageType::StaticClass()
            );
        }
    }
}

// ============================================================================
// [QTE Logic: Spear Throw]
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

    UE_LOG(LogTemp, Warning, TEXT("=== SPEAR QTE START! (Participants: %d) ==="), QTEProgressMap.Num());
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
    int32 FailCount = 0;

    for (auto& Pair : QTEProgressMap)
    {
        APlayerController* PC = Pair.Key;
        FPlayerQTEStatus& Status = Pair.Value;

        if (Status.PressCount != QTEGoalCount || Status.bFailed)
        {
            bAllSuccess = false;
            FailCount++;
            UE_LOG(LogTemp, Error, TEXT(">> Player %s FAILED! (Count: %d)"), *PC->GetName(), Status.PressCount);
        }
    }

    if (bAllSuccess)
    {
        UE_LOG(LogTemp, Warning, TEXT("=== QTE SUCCESS! Boss Stunned! ==="));
        if (!bPhase2Started) EnterPhase2();
        else SpawnDeathWall();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("=== QTE FAILED! Performing Wipe Attack! ==="));
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
                UGameplayStatics::ApplyDamage(
                    TargetPawn, 1000.0f, GetController(), this, UTrueDamageType::StaticClass()
                );
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
        UE_LOG(LogTemp, Warning, TEXT("=== DEATH WALL ACTIVATED! ==="));
    }
}