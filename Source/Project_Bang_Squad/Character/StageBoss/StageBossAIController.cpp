// Source/Project_Bang_Squad/Character/StageBoss/StageBossAIController.cpp

#include "StageBossAIController.h"
#include "Stage1Boss.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "GameFramework/Character.h"
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"
#include "DrawDebugHelpers.h"
#include "Components/CapsuleComponent.h" // 캡슐 컴포넌트 헤더 필요


AStageBossAIController::AStageBossAIController()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AStageBossAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    OwnerBoss = Cast<AStage1Boss>(InPawn);
    SetState(EBossAIState::Idle);
}

void AStageBossAIController::OnUnPossess()
{
    Super::OnUnPossess();
    ClearFocus(EAIFocusPriority::Gameplay);
}

void AStageBossAIController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!IsValid(OwnerBoss)) return;

    UHealthComponent* HealthComp = OwnerBoss->FindComponentByClass<UHealthComponent>();
    if (HealthComp && HealthComp->IsDead())
    {
        StopMovement();
        ClearFocus(EAIFocusPriority::Gameplay);
        return;
    }

    if (CurrentState != EBossAIState::Idle && !IsTargetValid())
    {
        SetState(EBossAIState::SwitchTarget);
    }

    switch (CurrentState)
    {
    case EBossAIState::Idle:        HandleIdle(DeltaTime); break;
    case EBossAIState::Chase:       HandleChase(DeltaTime); break;
    case EBossAIState::MeleeAttack: HandleMeleeAttack(DeltaTime); break;
    case EBossAIState::Retreat:     HandleRetreat(DeltaTime); break;
    case EBossAIState::RangeAttack: HandleRangeAttack(DeltaTime); break;
    case EBossAIState::SwitchTarget: break;
    }
}

void AStageBossAIController::SetState(EBossAIState NewState)
{
    CurrentState = NewState;
    StateTimer = 0.0f;
    AttackCooldownTimer = 0.0f;

    // --- [State Entry Logic] ---
    switch (CurrentState)
    {
    case EBossAIState::Chase:
        if (TargetActor) SetFocus(TargetActor);
        break;

    case EBossAIState::Retreat:
    {
        // [핵심 변경] 플레이어를 보지 마라 (어리버리 방지)
        ClearFocus(EAIFocusPriority::Gameplay);
        StopMovement();

        // 현재 보고 있는 방향의 '정반대'로 달린다.
        FVector RetreatDest = GetStraightRetreatLocation();

        // 이동 명령 (가속/회전 고려하여 넉넉한 수용 반경 설정)
        MoveToLocation(RetreatDest, 50.0f, true, true, true, true, 0, true);

        UE_LOG(LogTemp, Warning, TEXT("[BossAI] Straight Retreat Started!"));
    }
    break;

    case EBossAIState::RangeAttack:
        // [핵심] 이동이 끝났으니 다시 플레이어를 쳐다봐라 (뒤돌기)
        StopMovement();
        if (TargetActor)
        {
            SetFocus(TargetActor);
            // 즉시 회전하도록 강제하고 싶다면 아래 코드 추가 가능
            // FVector Dir = TargetActor->GetActorLocation() - OwnerBoss->GetActorLocation();
            // Dir.Z = 0.0f;
            // OwnerBoss->SetActorRotation(Dir.Rotation());
        }

        CurrentAttackCount = 0;
        AttackCooldownTimer = 1.0f; // 돌아서는 시간 고려하여 1초 뒤 발사
        break;

    case EBossAIState::MeleeAttack:
        StopMovement();
        if (TargetActor) SetFocus(TargetActor);
        CurrentAttackCount = 0;
        AttackCooldownTimer = 0.0f;
        break;

    case EBossAIState::SwitchTarget:
    {
        TArray<AActor*> Candidates;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), Candidates);

        TArray<AActor*> ValidTargets;
        for (AActor* Actor : Candidates)
        {
            if (Actor != OwnerBoss && Actor != TargetActor) ValidTargets.Add(Actor);
        }

        if (ValidTargets.Num() > 0)
        {
            TargetActor = ValidTargets[FMath::RandRange(0, ValidTargets.Num() - 1)];
        }

        // 타겟 변경 후 바로 추격
        SetState(EBossAIState::Chase);
    }
    break;
    }
}

// --- [State Handlers] ---

void AStageBossAIController::HandleIdle(float DeltaTime)
{
    TargetActor = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (IsTargetValid()) SetState(EBossAIState::Chase);
}

// ============================================================================
// [수정 1] 추적 로직: 높이(Z) 무시하고 수평 거리만 계산
// ============================================================================
void AStageBossAIController::HandleChase(float DeltaTime)
{
    if (!IsTargetValid()) return;

    // [변경 전] 3D 거리 계산 (키 차이 때문에 멀다고 착각함)
    // float Dist = OwnerBoss->GetDistanceTo(TargetActor);

    // [변경 후] 2D 수평 거리 계산 (높이 무시)
    FVector BossLoc = OwnerBoss->GetActorLocation();
    FVector TargetLoc = TargetActor->GetActorLocation();

    // Z축을 0으로 만들어 수평 거리만 남김
    float Dist2D = FVector::DistXY(BossLoc, TargetLoc);

    // 디버깅: 실제 인식하는 거리 확인 (로그로 확인해보세요)
    // UE_LOG(LogTemp, Warning, TEXT("Dist2D: %.2f / Trigger: %.2f"), Dist2D, AttackTriggerRange);

    // 공격 사거리 내 진입 시
    if (Dist2D <= AttackTriggerRange)
    {
        StopMovement(); // 확실하게 멈춤
        SetState(EBossAIState::MeleeAttack);
        return;
    }

    // 계속 추적 (바짝 붙도록)
    MoveToActor(TargetActor, MoveStopRange);
}

void AStageBossAIController::HandleMeleeAttack(float DeltaTime)
{
    if (AttackCooldownTimer > 0.0f)
    {
        AttackCooldownTimer -= DeltaTime;
        return;
    }

    OwnerBoss->DoAttack_Swing();
    CurrentAttackCount++;

    if (CurrentAttackCount >= MaxComboCount)
    {
        // 3타 후 후퇴로 전환 (딜레이 1.2초)
        FTimerHandle Handle;
        GetWorldTimerManager().SetTimer(Handle, [this]()
            {
                if (IsValid(this)) SetState(EBossAIState::Retreat);
            }, 1.2f, false);

        AttackCooldownTimer = 999.0f;
    }
    else
    {
        AttackCooldownTimer = AttackInterval;
    }
}

void AStageBossAIController::HandleRetreat(float DeltaTime)
{
    // 여기선 그냥 시간만 잰다 (이동은 SetState에서 이미 실행됨)
    StateTimer += DeltaTime;

    // 2초 동안 뒤로 달리고 나서 원거리 공격으로 전환
    if (StateTimer >= RetreatDuration)
    {
        SetState(EBossAIState::RangeAttack);
    }
}

void AStageBossAIController::HandleRangeAttack(float DeltaTime)
{
    // 타겟 바라보기 유지
    if (TargetActor) SetFocus(TargetActor);

    if (AttackCooldownTimer > 0.0f)
    {
        AttackCooldownTimer -= DeltaTime;
        return;
    }

    OwnerBoss->DoAttack_Slash();
    CurrentAttackCount++;

    if (CurrentAttackCount >= MaxComboCount)
    {
        // 3발 쏘고 타겟 변경 (딜레이 1.5초)
        FTimerHandle Handle;
        GetWorldTimerManager().SetTimer(Handle, [this]()
            {
                if (IsValid(this)) SetState(EBossAIState::SwitchTarget);
            }, 1.5f, false);

        AttackCooldownTimer = 999.0f;
    }
    else
    {
        AttackCooldownTimer = AttackInterval;
    }
}

// --- [Helper Functions] ---

// ============================================================================
// [수정 2] 후퇴 로직: 벽이 있으면 벽 앞까지만 도망감 (스마트 후퇴)
// ============================================================================
FVector AStageBossAIController::GetStraightRetreatLocation() const
{
    if (!OwnerBoss) return FVector::ZeroVector;

    FVector Origin = OwnerBoss->GetActorLocation();
    FVector BackwardDir = -OwnerBoss->GetActorForwardVector(); // 등 뒤 방향

    // 1. 뒤쪽에 벽이 있는지 레이저를 쏴서 확인 (충돌 체크)
    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerBoss); // 나는 무시

    // RetreatDistance만큼 뒤로 쏴봄
    bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        Origin,
        Origin + (BackwardDir * RetreatDistance),
        ECC_WorldStatic, // 벽(Static)만 체크
        Params
    );

    FVector FinalDest;

    if (bHit)
    {
        // 벽에 부딪혔다면? -> 벽에서 보스 덩치만큼 떨어진 곳까지만 이동
        float CapsuleRadius = OwnerBoss->GetCapsuleComponent()->GetScaledCapsuleRadius();
        float SafeBuffer = CapsuleRadius + 100.0f; // 덩치 + 여유공간

        // (벽 위치)에서 (앞으로 살짝) 이동
        FinalDest = HitResult.Location + (HitResult.Normal * SafeBuffer);

        // 디버그: 벽 감지됨
        if (bShowDebugLines)
        {
            DrawDebugLine(GetWorld(), Origin, HitResult.Location, FColor::Red, false, 2.0f, 0, 3.0f);
            DrawDebugSphere(GetWorld(), FinalDest, 30.0f, 12, FColor::Orange, false, 2.0f);
        }
        UE_LOG(LogTemp, Warning, TEXT("[BossAI] Wall Detected behind! Shortening retreat distance."));
    }
    else
    {
        // 벽이 없으면 원래대로 최대 거리 이동
        FinalDest = Origin + (BackwardDir * RetreatDistance);

        if (bShowDebugLines)
        {
            DrawDebugLine(GetWorld(), Origin, FinalDest, FColor::Green, false, 2.0f, 0, 3.0f);
        }
    }

    // 2. 네비게이션 위에 투영 (최종 갈 수 있는 땅인지 확인)
    FNavLocation NavLoc;
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());

    // Extent(검색 범위)를 넉넉하게(Z축 500) 줘서 위아래 층도 인식 가능하게 함
    if (NavSys && NavSys->ProjectPointToNavigation(FinalDest, NavLoc, FVector(500, 500, 500)))
    {
        return NavLoc.Location;
    }

    // 갈 곳이 정 없다면 현재 위치 반환 (이동 실패)
    UE_LOG(LogTemp, Error, TEXT("[BossAI] Retreat Path Blocked (NavMesh not found)!"));
    return Origin;
}
bool AStageBossAIController::IsTargetValid() const
{
    return (TargetActor && IsValid(TargetActor));
}