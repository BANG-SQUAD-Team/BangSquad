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
#include "Engine/TargetPoint.h" // TargetPoint 사용을 위해 포함
#include "BossSpikeTrap.h"
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
    // [Multiplayer Principle] 스폰은 오직 서버에서만
    if (!HasAuthority()) return;

    // [Safety] 스폰 포인트 설정 여부 확인
    if (CrystalSpawnPoints.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[Stage1Boss] No Crystal Spawn Points assigned! Please check the Level Instance properties."));
        return;
    }

    // 소환할 직업 순서 (Titan -> Striker -> Mage -> Paladin)
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

        // [Refactored Logic] 레벨에 배치된 TargetPoint의 절대 좌표(World Location) 사용
        ATargetPoint* SpawnPoint = CrystalSpawnPoints[i];

        // TargetPoint가 유효하지 않으면 스킵 (삭제되었거나 null인 경우)
        if (!IsValid(SpawnPoint))
        {
            UE_LOG(LogTemp, Warning, TEXT("[Stage1Boss] Spawn Point index %d is Invalid!"), i);
            continue;
        }

        // 보스의 회전값과 관계없이 고정된 월드 좌표 사용
        FVector SpawnLoc = SpawnPoint->GetActorLocation();
        // 회전값은 0으로 초기화하거나, 필요하다면 SpawnPoint->GetActorRotation() 사용
        FRotator SpawnRot = FRotator::ZeroRotator;

        FActorSpawnParameters Params;
        Params.Owner = this;
        Params.Instigator = this; // AI 인식 등을 위해 Instigator 설정
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

        // 찾은 BP로 소환
        AJobCrystal* NewCrystal = GetWorld()->SpawnActor<AJobCrystal>(TargetClass, SpawnLoc, SpawnRot, Params);
        if (NewCrystal)
        {
            NewCrystal->TargetBoss = this;
            NewCrystal->RequiredJobType = CurrentJob; // 직업 정보 주입
            RemainingGimmickCount++;

            UE_LOG(LogTemp, Log, TEXT("[Server] Spawned Crystal(%s) at %s"), *NewCrystal->GetName(), *SpawnLoc.ToString());
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Stage1Boss: Successfully Spawned %d Crystals."), RemainingGimmickCount);
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
    FRotator SpawnRot = GetActorRotation(); // 필요 시 이것도 고정 로테이션으로 변경 고려

    // 현재는 기존 FVector WallSpawnLocation(상대 좌표 widget)을 그대로 사용 중.
    // 만약 이것도 고정하고 싶다면 TargetPoint로 교체 필요.
    // 여기서는 보스의 위치 기준으로 WallSpawnLocation 오프셋을 적용하여 소환.
    FVector SpawnLoc = GetActorTransform().TransformPosition(WallSpawnLocation);

    ADeathWall* NewWall = GetWorld()->SpawnActor<ADeathWall>(DeathWallClass, SpawnLoc, SpawnRot, Params);
    if (NewWall)
    {
        NewWall->ActivateWall();
    }
}

void AStage1Boss::TrySpawnSpikeAtRandomPlayer()
{
    // [권한 분리]: 패턴 판단과 스폰은 서버에서만
    if (!HasAuthority()) return;

    // 1. 몽타주 재생 (모든 클라이언트에 보임)
    if (SpellMontage)
    {
        Multicast_PlaySpellMontage();
    }

    // 2. 생존해 있는 플레이어 캐릭터 찾기
    TArray<AActor*> FoundPlayers;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), FoundPlayers);

    // 플레이어 태그나 특정 클래스로 필터링이 필요할 수 있음
    TArray<AActor*> ValidPlayers;
    for (AActor* Actor : FoundPlayers)
    {
        // 보스 자신 제외, 죽은 플레이어 제외 등 조건 체크
        if (Actor != this && !Actor->IsHidden())
        {
            ValidPlayers.Add(Actor);
        }
    }

    if (ValidPlayers.Num() == 0) return;

    // 3. 랜덤 타겟 선정
    int32 RandomIndex = FMath::RandRange(0, ValidPlayers.Num() - 1);
    AActor* TargetPlayer = ValidPlayers[RandomIndex];

    if (TargetPlayer && SpikeTrapClass)
    {
        // 4. 플레이어 발밑 좌표 계산 (바닥 Z값 보정 필요 시 수정)
        FVector SpawnLocation = TargetPlayer->GetActorLocation();
        SpawnLocation.Z -= 90.0f; // 캡슐 절반 높이만큼 내려서 바닥에 붙임 (조정 필요)
        FRotator SpawnRotation = FRotator::ZeroRotator;

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.Instigator = this; // 데미지 처리를 위해 Instigator 설정

        // 5. 함정 스폰 (Replicated 액터이므로 클라에도 자동 생성됨)
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
// [1] 패턴 시작: 몽타주만 재생 (서버)
// 수정: 지역 변수 선언을 제거하고 멤버 변수 BossData를 직접 사용
void AStage1Boss::StartSpikePattern()
{
    // [권한 분리]
    if (!HasAuthority()) return;

    // 오류 수정: UEnemyBossData* BossData 선언 제거.
    // 클래스 멤버 변수 BossData를 바로 사용합니다.
    if (BossData && BossData->SpellMontage)
    {
        // 몽타주 재생 (Multicast로 자동 복제됨)
        PlayAnimMontage(BossData->SpellMontage);
    }
}
// [2] 실제 스폰: AnimNotify가 호출 (서버)
// 수정: 기존 TrySpawnSpikeAtRandomPlayer의 로직을 여기로 이식
void AStage1Boss::ExecuteSpikeSpell()
{
    // [권한 분리] 스폰은 오직 서버만!
    if (!HasAuthority()) return;

    // 1. 생존해 있는 플레이어 캐릭터 찾기
    TArray<AActor*> FoundPlayers;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), FoundPlayers);

    TArray<AActor*> ValidPlayers;
    for (AActor* Actor : FoundPlayers)
    {
        // 보스 자신 제외, 죽은 플레이어 제외 등 조건 체크
        if (Actor != this && !Actor->IsHidden())
        {
            ValidPlayers.Add(Actor);
        }
    }

    if (ValidPlayers.Num() == 0) return;

    // 2. 랜덤 타겟 선정
    int32 RandomIndex = FMath::RandRange(0, ValidPlayers.Num() - 1);
    AActor* TargetPlayer = ValidPlayers[RandomIndex];

    // 3. 함정 스폰
    if (TargetPlayer && SpikeTrapClass)
    {
        // 플레이어 발밑 좌표 계산 (바닥 Z값 보정 필요 시 수정, 예: -90)
        FVector SpawnLocation = TargetPlayer->GetActorLocation();
        SpawnLocation.Z -= 90.0f;
        FRotator SpawnRotation = FRotator::ZeroRotator;

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.Instigator = this; // 데미지 처리를 위해 Instigator 설정

        // Replicated 액터이므로 서버에서 스폰하면 클라에도 자동 생성됨
        GetWorld()->SpawnActor<ABossSpikeTrap>(SpikeTrapClass, SpawnLocation, SpawnRotation, SpawnParams);

        UE_LOG(LogTemp, Log, TEXT("Trap Spawned at %s by AnimNotify!"), *TargetPlayer->GetName());
    }
}