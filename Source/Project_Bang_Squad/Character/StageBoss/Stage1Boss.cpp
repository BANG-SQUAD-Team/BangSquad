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
#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h" // 팀킬 확인용
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

   
    if (HasAuthority())
    {
        // 기믹(무적)으로 시작
         SetPhase(EBossPhase::Gimmick); 

        // [TEST] 데미지 확인용: 1페이즈(전투 상태)로 강제 시작
       // SetPhase(EBossPhase::Phase1);

        UE_LOG(LogTemp, Warning, TEXT("[TEST] Stage1Boss: Force Started in Phase 1 for Damage Test"));
    }
}
// ============================================================================
// [Phase & Damage Logic]
// ============================================================================

float AStage1Boss::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    // 1. [서버 권한] 데미지 계산은 오직 서버에서만
    if (!HasAuthority()) return 0.0f;

    // 2. [팀킬 방지] 몬스터끼리는 데미지 무효화
    AActor* Attacker = DamageCauser;
    if (EventInstigator && EventInstigator->GetPawn())
    {
        Attacker = EventInstigator->GetPawn();
    }
    else if (DamageCauser && DamageCauser->GetOwner()) // 투사체인 경우
    {
        Attacker = DamageCauser->GetOwner();
    }

    if (Attacker)
    {
        // 자기 자신이거나, 같은 몬스터(EnemyCharacterBase 상속)라면 무시
        if (Attacker == this) return 0.0f;
        if (Attacker->IsA(AEnemyCharacterBase::StaticClass()))
        {
            // UE_LOG(LogTemp, Log, TEXT("Friendly Fire prevented on Boss."));
            return 0.0f;
        }
    }

    // 3. 부모 로직 실행 (기본 처리)
    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    // 4. [HealthComponent 연동] 명시적 처리
    // (만약 부모 클래스인 StageBossBase가 HealthComponent 처리를 안 한다면 여기서 필수)
    UHealthComponent* HC = FindComponentByClass<UHealthComponent>();
    if (HC && !HC->IsDead())
    {
        // 이미 부모나 내부 로직에서 깎였는지 확인 후 적용
        // 여기서는 안전하게 ApplyDamage를 호출하여 델리게이트를 발동시킴
        // 주의: 중복 차감을 막기 위해 HC 내부 로직 확인 필요하나, 
        // 보통은 여기서 ApplyDamage를 호출하는 것이 정석입니다.
        HC->ApplyDamage(ActualDamage);
    }
    else
    {
        return 0.0f; // 체력 컴포넌트 없거나 죽었으면 데미지 무효
    }

    // 5. [페이즈 전환 체크]
    // 체력이 깎인 "후"의 상태를 검사
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

// [기존 유지하되 주석 추가]
void AStage1Boss::AnimNotify_SpawnSlash()
{
    // [서버 권한 필수] 투사체는 서버에서 생성 후 리플리케이트됨
    if (!HasAuthority()) return;

    // 데이터 에셋 체크
    if (!BossData || !BossData->SlashProjectileClass)
    {
        UE_LOG(LogTemp, Error, TEXT("BossData or SlashProjectileClass is NULL!"));
        return;
    }

    // 위치 계산: 보스 정면 1.5m 앞
    FVector SpawnLocation = GetActorLocation() + GetActorForwardVector() * 150.0f;
    FRotator SpawnRotation = GetActorRotation(); // 보스가 보는 방향

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this; // 데미지 판정 시 주인이 됨

    ASlashProjectile* Projectile = GetWorld()->SpawnActor<ASlashProjectile>(
        BossData->SlashProjectileClass,
        SpawnLocation,
        SpawnRotation,
        SpawnParams
    );

    if (Projectile)
    {
        // 필요 시 투사체 초기 설정 (예: 타겟 설정 등)
        // UE_LOG(LogTemp, Log, TEXT("Boss Slash Projectile Spawned!"));
    }
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