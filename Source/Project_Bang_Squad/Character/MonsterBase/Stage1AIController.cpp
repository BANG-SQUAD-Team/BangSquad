// Source/Project_Bang_Squad/Character/MonsterBase/Stage1AIController.cpp

#include "Stage1AIController.h"
#include "Project_Bang_Squad/Character/Enemy/EnemyMidBoss.h"
#include "TimerManager.h"

void AStage1AIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    MyPawn = Cast<AEnemyMidBoss>(InPawn);

    // [수정 1] 하드코딩 제거! 
    // 이제 에디터(블루프린트)의 AttackRange 값을 따릅니다.
    // 만약 값이 너무 작으면 안전장치로 기본값 설정
    if (AttackRange <= 50.0f)
    {
        AttackRange = 200.0f;
    }

    LastSlashTime = -SlashSkillCooldown;
}

// [핵심 로직] 행동 결정 (Decision Making)
// 부모 클래스의 Tick 등에서 StartChasing을 호출할 때 이 함수가 대신 실행됩니다.
void AStage1AIController::StartChasing()
{
    // 예외 처리
    if (CurrentAIState == EMidBossAIState::Dead) return;
    if (!TargetActor || !MyPawn) return;

    float Distance = MyPawn->GetDistanceTo(TargetActor);
    float CurrentTime = GetWorld()->GetTimeSeconds();
    bool bCanSlash = (CurrentTime - LastSlashTime >= SlashSkillCooldown);

    // --- [1. 패턴 결정 (Brain)] ---

    // A. 너무 가까우면(300cm 이하) -> 무조건 근접 공격 (참격 쏘려다 맞음)
    if (Distance <= 300.0f)
    {
        CurrentPattern = EStage1Pattern::MeleeRush;
    }
    // B. 사거리 밖이고(1200cm 이내) + 쿨타임 찼으면 -> 확률적으로 참격
    else if (bCanSlash && Distance <= 1200.0f)
    {
        // 40% 확률로 참격, 60% 확률로 그냥 돌진 (난이도 조절 가능)
        if (FMath::RandRange(0, 100) < 40)
        {
            CurrentPattern = EStage1Pattern::SlashStationary;
        }
        else
        {
            CurrentPattern = EStage1Pattern::MeleeRush;
        }
    }
    // C. 그 외 (너무 멀거나 쿨타임 안 찼음) -> 돌진
    else
    {
        CurrentPattern = EStage1Pattern::MeleeRush;
    }

    // --- [2. 행동 실행 (Action)] ---

    if (CurrentPattern == EStage1Pattern::SlashStationary)
    {
        // [참격 선택 시]: 쫓아가지 않고(Stop), 그 자리에서 즉시 발사!
        StopMovement();
        CurrentAIState = EMidBossAIState::Attack;
        ClearFocus(EAIFocusPriority::Gameplay); // 회전 자유롭게

        // 타겟 방향으로 즉시 몸 돌리기
        FVector Direction = TargetActor->GetActorLocation() - MyPawn->GetActorLocation();
        Direction.Z = 0.0f;
        if (!Direction.IsNearlyZero())
        {
            MyPawn->SetActorRotation(Direction.Rotation());
        }

        UE_LOG(LogTemp, Warning, TEXT("[AI Decision] Slash Selected! (Stationary Attack)"));

        // 공격 실행
        LastSlashTime = CurrentTime;
        float AnimDuration = MyPawn->PlaySlashAttack();
        float WaitTime = (AnimDuration > 0.f) ? AnimDuration : 1.5f;

        // 애니메이션이 끝날 때쯤 복귀(FinishAttack) 예약
        GetWorld()->GetTimerManager().SetTimer(StateTimerHandle, this, &AMidBossAIController::FinishAttack, WaitTime, false);
    }
    else
    {
        // [근접 돌진 선택 시]: 이제서야 추격 상태로 진입
        UE_LOG(LogTemp, Log, TEXT("[AI Decision] Melee Rush Selected! Chasing..."));

        CurrentAIState = EMidBossAIState::Chase;

        // 이동 시작 (SetFocus로 타겟을 계속 쳐다보며 이동)
        MoveToActor(TargetActor, AttackRange - 10.0f);
        SetFocus(TargetActor);
    }
}

void AStage1AIController::UpdateChaseState(float DeltaTime)
{
    if (!TargetActor || !MyPawn) return;

    float Distance = MyPawn->GetDistanceTo(TargetActor);

    // [공격 사거리 도달 체크]
    // 중심점 거리(Distance)가 AttackRange보다 작으면 멈춤 -> 아주 정확함
    if (Distance <= AttackRange)
    {
        // 1. 즉시 정지
        StopMovement();

        // 2. 공격 상태 전환
        CurrentAIState = EMidBossAIState::Attack;
        ClearFocus(EAIFocusPriority::Gameplay);

        UE_LOG(LogTemp, Log, TEXT("[AI Action] Reached Target -> Attack!"));

        float AnimDuration = MyPawn->PlayRandomAttack();
        float WaitTime = (AnimDuration > 0.f) ? AnimDuration : 1.0f;
        GetWorld()->GetTimerManager().SetTimer(StateTimerHandle, this, &AMidBossAIController::FinishAttack, WaitTime, false);
    }
    else
    {
        // [수정 2] 이동 명령의 허용 반경(AcceptanceRadius)을 아주 작게(5~20) 설정
        // "사거리보다 훨씬 더 안쪽까지 파고들어라!"고 명령해야
        // 위의 'Distance <= AttackRange' 조건에 걸려서 Tick에서 멈춥니다.

        SetFocus(TargetActor);

        // AttackRange - 10.0f (X) -> 20.0f (O)
        // 20cm까지 붙으라고 시켜야 중간에 AttackRange(200cm)에서 StopMovement()가 걸립니다.
        MoveToActor(TargetActor, 20.0f);
    }
}

void AStage1AIController::UpdateAttackState(float DeltaTime)
{
    // 공격 애니메이션 중에도 타겟을 향해 아주 부드럽게 회전 보정 (유도 성능)
    // 너무 팍팍 돌면 어색하므로 InterpSpeed를 낮게 설정
    if (TargetActor && MyPawn)
    {
        FVector Direction = TargetActor->GetActorLocation() - MyPawn->GetActorLocation();
        Direction.Z = 0.0f;
        FRotator TargetRot = Direction.Rotation();
        MyPawn->SetActorRotation(FMath::RInterpTo(MyPawn->GetActorRotation(), TargetRot, DeltaTime, 5.0f));
    }
}