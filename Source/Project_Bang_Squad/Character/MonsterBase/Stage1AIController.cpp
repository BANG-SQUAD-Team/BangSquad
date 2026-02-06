// Source/Project_Bang_Squad/Character/MonsterBase/Stage1AIController.cpp

#include "Stage1AIController.h"
#include "Project_Bang_Squad/Character/Enemy/EnemyMidBoss.h"
#include "TimerManager.h"

void AStage1AIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    MyPawn = Cast<AEnemyMidBoss>(InPawn);

    if (AttackRange <= 50.0f) AttackRange = 200.0f;

    // [초기화] 첫 시작은 평타(false)부터 시작 (원하시면 true로 변경 가능)
    bIsSlashTurn = false;
}

// [핵심 로직] "이번엔 네 차례다"
void AStage1AIController::StartChasing()
{
    if (CurrentAIState == EMidBossAIState::Dead) return;
    if (!TargetActor || !MyPawn) return;

    // 1. 순서에 따라 패턴 강제 지정 (확률 X)
    if (bIsSlashTurn)
    {
        CurrentPattern = EStage1Pattern::SlashStationary;
        UE_LOG(LogTemp, Warning, TEXT("[Pattern] Turn: SLASH (Target Dist: 700)"));
    }
    else
    {
        CurrentPattern = EStage1Pattern::MeleeRush;
        UE_LOG(LogTemp, Warning, TEXT("[Pattern] Turn: MELEE (Target Dist: %f)"), AttackRange);
    }

    // 2. 추격 상태로 전환 (실제 이동과 공격 판정은 UpdateChaseState에서 처리)
    CurrentAIState = EMidBossAIState::Chase;
}

// [이동 로직] 패턴에 따라 "얼마나 가까이 갈지" 결정
void AStage1AIController::UpdateChaseState(float DeltaTime)
{
    if (!TargetActor || !MyPawn) return;

    float Distance = MyPawn->GetDistanceTo(TargetActor);

    // 패턴별 사거리 설정
    // - 평타: AttackRange (보통 150~200)
    // - 참격: 700 (너무 멀면 쏘기 애매하므로 적당히 접근)
    float RequiredDistance = (CurrentPattern == EStage1Pattern::SlashStationary) ? 700.0f : AttackRange;

    // 1. 사거리 충족 시 -> 공격 실행
    if (Distance <= RequiredDistance)
    {
        StopMovement();
        CurrentAIState = EMidBossAIState::Attack;
        ClearFocus(EAIFocusPriority::Gameplay);

        float AnimDuration = 0.0f;

        // 패턴별 공격 실행
        if (CurrentPattern == EStage1Pattern::SlashStationary)
        {
            // 타겟 방향 바라보기 (보정)
            FVector Dir = TargetActor->GetActorLocation() - MyPawn->GetActorLocation();
            Dir.Z = 0.f;
            MyPawn->SetActorRotation(Dir.Rotation());

            AnimDuration = MyPawn->PlaySlashAttack(); // 참격 발사
        }
        else
        {
            AnimDuration = MyPawn->PlayRandomAttack(); // 평타
        }

        // 애니메이션 실패 방지
        if (AnimDuration <= 0.f) AnimDuration = 1.0f;

        // 공격 끝나면 FinishAttack 호출
        GetWorld()->GetTimerManager().SetTimer(StateTimerHandle, this, &AStage1AIController::FinishAttack, AnimDuration, false);
    }
    // 2. 사거리 밖 -> 이동
    else
    {
        // 타겟을 바라보며 이동
        SetFocus(TargetActor);
        MoveToActor(TargetActor, 5.0f);
    }
}

// [순서 교체] 공격이 끝날 때마다 플래그 뒤집기
void AStage1AIController::FinishAttack()
{
    if (CurrentAIState == EMidBossAIState::Dead) return;

    // true -> false -> true -> false (무한 반복)
    bIsSlashTurn = !bIsSlashTurn;

    UE_LOG(LogTemp, Log, TEXT("[Cycle] Attack Finished. Next Turn Slash? %s"), bIsSlashTurn ? TEXT("YES") : TEXT("NO"));

    // 다음 행동 즉시 시작
    StartChasing();
}

// (회전 보정은 기존 코드 유지)
void AStage1AIController::UpdateAttackState(float DeltaTime)
{
    if (TargetActor && MyPawn)
    {
        FVector Direction = TargetActor->GetActorLocation() - MyPawn->GetActorLocation();
        Direction.Z = 0.0f;
        FRotator TargetRot = Direction.Rotation();
        MyPawn->SetActorRotation(FMath::RInterpTo(MyPawn->GetActorRotation(), TargetRot, DeltaTime, 5.0f));
    }
}