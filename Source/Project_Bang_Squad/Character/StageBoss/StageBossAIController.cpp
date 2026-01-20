// Source/Project_Bang_Squad/Character/StageBoss/StageBossAIController.cpp

#include "StageBossAIController.h"
#include "Stage1Boss.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "GameFramework/Character.h"
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"

AStageBossAIController::AStageBossAIController()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AStageBossAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    OwnerBoss = Cast<AStage1Boss>(InPawn);

    // 초기화 시 Idle로 시작
    SetState(EBossAIState::Idle);
}

void AStageBossAIController::OnUnPossess()
{
    Super::OnUnPossess();
    // 포커스 해제 안 하면 다음 스폰된 폰이 이상한 곳을 볼 수 있음
    ClearFocus(EAIFocusPriority::Gameplay);
}

void AStageBossAIController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 1. 보스 유효성 검사
    if (!IsValid(OwnerBoss)) return;

    // 2. 보스 사망 체크 (HealthComponent 활용)
    UHealthComponent* HealthComp = OwnerBoss->FindComponentByClass<UHealthComponent>();
    if (HealthComp && HealthComp->IsDead())
    {
        StopMovement();
        ClearFocus(EAIFocusPriority::Gameplay);
        return; // AI 정지
    }

    // 3. 타겟 유효성 검사 (타겟이 로그아웃하거나 죽었으면 재탐색)
    if (CurrentState != EBossAIState::Idle && !IsTargetValid())
    {
        SetState(EBossAIState::SwitchTarget);
    }

    // 4. 상태 머신 실행
    switch (CurrentState)
    {
    case EBossAIState::Idle:        HandleIdle(DeltaTime); break;
    case EBossAIState::Chase:       HandleChase(DeltaTime); break;
    case EBossAIState::MeleeAttack: HandleMeleeAttack(DeltaTime); break;
    case EBossAIState::Retreat:     HandleRetreat(DeltaTime); break;
    case EBossAIState::RangeAttack: HandleRangeAttack(DeltaTime); break;
    case EBossAIState::SwitchTarget:
        // 틱에서 호출할 필요 없이 SetState에서 처리하고 바로 전환
        break;
    }
}

void AStageBossAIController::SetState(EBossAIState NewState)
{
    CurrentState = NewState;
    StateTimer = 0.0f;
    AttackCooldownTimer = 0.0f; // 상태 변경 시 쿨타임 초기화 (필요시 조정)

    // [OOP] 상태 패턴: 진입(Enter) 로직
    switch (CurrentState)
    {
    case EBossAIState::Chase:
        // 추적 시작 시 타겟을 바라보게 함
        if (TargetActor) SetFocus(TargetActor);
        break;

    case EBossAIState::Retreat:
    {
        // [중요] 후퇴할 때는 타겟을 바라보면 안 됨 (문워크 방지) -> Focus 해제
        ClearFocus(EAIFocusPriority::Gameplay);
        StopMovement(); // 기존 이동 명령 취소

        FVector Dest = GetRetreatLocation();
        MoveToLocation(Dest, -1.0f, true, true, true, true, 0, true);

        UE_LOG(LogTemp, Log, TEXT("[BossAI] Retreating to: %s"), *Dest.ToString());
    }
    break;

    case EBossAIState::RangeAttack:
        // 원거리 공격 시 다시 타겟 조준
        CurrentAttackCount = 0;
        AttackCooldownTimer = 0.5f; // 진입 후 0.5초 뒤 첫 발사
        if (TargetActor) SetFocus(TargetActor);
        StopMovement(); // 이동 정지
        break;

    case EBossAIState::MeleeAttack:
        CurrentAttackCount = 0;
        AttackCooldownTimer = 0.0f; // 즉시 공격
        StopMovement();
        if (TargetActor) SetFocus(TargetActor);
        break;

    case EBossAIState::SwitchTarget:
    {
        // 타겟 재설정 로직
        TArray<AActor*> Candidates;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), Candidates);

        TArray<AActor*> ValidTargets;
        for (AActor* Actor : Candidates)
        {
            // 나 자신 제외, 죽은 애들 제외 로직 추가 가능
            if (Actor != OwnerBoss)
            {
                ValidTargets.Add(Actor);
            }
        }

        if (ValidTargets.Num() > 0)
        {
            // 완전 랜덤 선택
            int32 RandIdx = FMath::RandRange(0, ValidTargets.Num() - 1);
            TargetActor = ValidTargets[RandIdx];
            UE_LOG(LogTemp, Warning, TEXT("[BossAI] Target Switched to: %s"), *TargetActor->GetName());
        }

        // 타겟 변경 후 바로 추격으로 전환
        SetState(EBossAIState::Chase);
    }
    break;
    }
}

void AStageBossAIController::HandleIdle(float DeltaTime)
{
    // 가장 가까운 플레이어 탐색
    TargetActor = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (IsTargetValid())
    {
        SetState(EBossAIState::Chase);
    }
}

void AStageBossAIController::HandleChase(float DeltaTime)
{
    if (!IsTargetValid()) return;

    // 거리 계산 (Capsule끼리의 거리 아님, Actor 중심점 거리)
    float Dist = OwnerBoss->GetDistanceTo(TargetActor);

    // [수정 포인트 1] 공격 범위(300)보다 안쪽이면 공격 상태로 전환
    if (Dist <= AttackTriggerRange)
    {
        SetState(EBossAIState::MeleeAttack);
        return;
    }

    // [수정 포인트 2] 이동 명령
    // MoveToActor는 내부적으로 경로 최적화를 하므로 매 틱 호출해도 괜찮으나,
    // 성능을 위해 EPathFollowingRequestResult를 체크하거나 타이머를 쓸 수 있음.
    // 여기서는 "멈추는 거리(200)"를 공격 범위(300)보다 확실히 작게 줘서, 
    // 도착하면 무조건 공격 범위 안에 있게 만듭니다.
    MoveToActor(TargetActor, MoveStopRange);
}

void AStageBossAIController::HandleMeleeAttack(float DeltaTime)
{
    // 쿨타임 체크
    if (AttackCooldownTimer > 0.0f)
    {
        AttackCooldownTimer -= DeltaTime;
        return;
    }

    // 공격 실행 (서버에서 실행 -> Boss.cpp의 Multicast로 전파됨)
    OwnerBoss->DoAttack_Swing();
    CurrentAttackCount++;

    // 콤보 체크
    if (CurrentAttackCount >= MaxComboCount)
    {
        // 공격 후딜레이(1.0f)를 준 뒤 후퇴 상태로 전환
        // 람다 캡처 시 this 안전성 주의 (타이머 핸들 관리 필요하지만 약식으로)
        FTimerHandle Handle;
        GetWorldTimerManager().SetTimer(Handle, [this]()
            {
                if (IsValid(this)) SetState(EBossAIState::Retreat);
            }, 1.2f, false);

        // 대기 중 중복 실행 방지
        AttackCooldownTimer = 999.0f;
    }
    else
    {
        // 다음 공격까지 대기
        AttackCooldownTimer = AttackInterval;
    }
}

void AStageBossAIController::HandleRetreat(float DeltaTime)
{
    StateTimer += DeltaTime;

    // [수정 포인트 3] 후퇴 중에는 Focus를 끄고 이동만 집중
    // MoveToLocation이 이미 실행 중이므로 여기선 시간만 잽니다.

    // 2초 지나면 원거리 공격 패턴으로
    if (StateTimer >= RetreatDuration)
    {
        SetState(EBossAIState::RangeAttack);
    }
}

void AStageBossAIController::HandleRangeAttack(float DeltaTime)
{
    // 원거리 공격은 타겟을 계속 바라봐야 함 (이미 SetState에서 SetFocus 함)

    if (AttackCooldownTimer > 0.0f)
    {
        AttackCooldownTimer -= DeltaTime;
        return;
    }

    // 발사
    OwnerBoss->DoAttack_Slash();
    CurrentAttackCount++;

    if (CurrentAttackCount >= MaxComboCount)
    {
        // 3발 다 쏘면 타겟 변경
        FTimerHandle Handle;
        GetWorldTimerManager().SetTimer(Handle, [this]()
            {
                if (IsValid(this)) SetState(EBossAIState::SwitchTarget);
            }, 1.5f, false); // 마지막 발사 후 1.5초 대기

        AttackCooldownTimer = 999.0f;
    }
    else
    {
        AttackCooldownTimer = AttackInterval;
    }
}

// --- Helper Functions ---

bool AStageBossAIController::IsTargetValid() const
{
    if (!TargetActor || !IsValid(TargetActor)) return false;

    // (옵션) 타겟이 죽었는지 체크
    /*
    auto* TargetHC = TargetActor->FindComponentByClass<UHealthComponent>();
    if (TargetHC && TargetHC->IsDead()) return false;
    */

    return true;
}

FVector AStageBossAIController::GetRetreatLocation() const
{
    if (!TargetActor || !OwnerBoss) return FVector::ZeroVector;

    // 플레이어 -> 보스 방향 벡터 (도망갈 방향)
    FVector Direction = (OwnerBoss->GetActorLocation() - TargetActor->GetActorLocation()).GetSafeNormal();

    // Z축 영향 제거 (평면 이동)
    Direction.Z = 0.0f;
    if (Direction.IsNearlyZero()) Direction = -OwnerBoss->GetActorForwardVector(); // 겹쳐있으면 뒤로

    FVector Origin = OwnerBoss->GetActorLocation();
    FVector DesiredLoc = Origin + (Direction * RetreatDistance);

    // 네비게이션 시스템 위로 투영 (갈 수 있는 길인지 확인)
    FNavLocation NavLoc;
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());

    if (NavSys && NavSys->ProjectPointToNavigation(DesiredLoc, NavLoc))
    {
        return NavLoc.Location;
    }

    // 만약 뒤가 막혔다면? (벽 등) -> 그냥 제자리거나 약간 옆으로 (여기선 간단히 Origin 반환 or 벽 슬라이딩)
    // 개선안: EQS를 써야 가장 좋지만, 코드 상으로는 45도씩 꺾어서 재검사 하는 로직 추가 가능.
    // 일단은 갈 수 있는 최대한으로 투영 시도
    return DesiredLoc;
}