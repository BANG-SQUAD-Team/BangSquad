// Source/Project_Bang_Squad/Character/MonsterBase/Stage2AIController.cpp

#include "Stage2AIController.h"
#include "Project_Bang_Squad/Character/Enemy/Stage2MidBoss.h"
#include "TimerManager.h"

void AStage2AIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    MyPawn = Cast<AStage2MidBoss>(InPawn);

    // 기본 설정
    //SetAttackRange(800.0f);
    // // 대신 초기값 0이면 안전장치로 세팅 (선택 사항)
    if (AttackRange <= 0.0f)
    {
        AttackRange = 800.0f;
    }
    MagicFireCount = 0;
    CurrentPattern = EStage2Pattern::MagicBarrage; // 시작은 마법부터
}

void AStage2AIController::Tick(float DeltaTime)
{
    // 부모 Tick 로직 (타겟 유효성 검사 등)
    if (!TargetActor || IsTargetDead(TargetActor))
    {
        CurrentAIState = EMidBossAIState::Idle;
        StopMovement();
        return;
    }

    // 마법사 전용 패턴 실행
    UpdateMagePattern(DeltaTime);
}

void AStage2AIController::StartChasing()
{
    // [Bug Fix] 피격(Hit) 상태에서 복귀하는 경우인지 체크
    // OnDamaged에서 StartChasing을 호출할 때, CurrentAIState는 아직 'Hit' 상태입니다.
    bool bResumingFromHit = (CurrentAIState == EMidBossAIState::Hit);

    CurrentAIState = EMidBossAIState::Attack;
    StopMovement();

    // [핵심 로직] 
    // "아예 새로 추격을 시작할 때"만 패턴을 0으로 초기화합니다.
    // "맞고 나서 정신 차린 경우"라면 하던 짓(변수들)을 그대로 유지합니다.
    if (!bResumingFromHit)
    {
        CurrentPattern = EStage2Pattern::MagicBarrage;
        MagicFireCount = 0;

        // 처음 만났을 때는 0.5초 노려보기 (회전 보정용)
        PatternCooldown = 0.5f;
    }
    // else: 맞고 돌아온 경우에는 PatternCooldown을 설정하지 않아(0.0), 
    // 즉시(Immediate) 다음 공격을 이어가게 하여 플레이어를 압박합니다.
}
void AStage2AIController::UpdateMagePattern(float DeltaTime)
{
    if (GetWorld()->GetTimerManager().IsTimerActive(StateTimerHandle)) return;

    // [추가] 쿨타임 동안 타겟 바라보기 (이 코드가 실행될 시간을 벌어줘야 함)
    if (PatternCooldown > 0.0f)
    {
        PatternCooldown -= DeltaTime;

        if (TargetActor && MyPawn)
        {
            FVector Dir = TargetActor->GetActorLocation() - MyPawn->GetActorLocation();
            Dir.Z = 0.f;

            // 부드럽게 타겟 방향으로 회전
            FRotator TargetRot = Dir.Rotation();
            MyPawn->SetActorRotation(FMath::RInterpTo(MyPawn->GetActorRotation(), TargetRot, DeltaTime, 10.0f));
        }
        return; // 회전 중에는 다른 행동 안 함
    }

    // --- 패턴 실행 ---
    switch (CurrentPattern)
    {
    case EStage2Pattern::MagicBarrage:
        if (MagicFireCount < 3)
        {
            StopMovement();

            // 발사! (이제 보스는 플레이어를 보고 있을 확률이 높음)
            MyPawn->FireMagicMissile();
            float AnimTime = MyPawn->PlayMagicAttackAnim();

            // 다음 발사까지 대기 (이 시간 동안 또 플레이어를 노려보며 회전함)
            float WaitTime = (AnimTime > 0.f) ? AnimTime : 1.0f;
            GetWorld()->GetTimerManager().SetTimer(StateTimerHandle, this, &AMidBossAIController::FinishAttack, WaitTime, false);

            PatternCooldown = 0.5f; // 연사 간격 (이때 회전 보정됨)
            MagicFireCount++;
        }
        else
        {
            CurrentPattern = EStage2Pattern::Teleport;
            PatternCooldown = 1.0f; // 텔포 전 뜸들이기
        }
        break;

    case EStage2Pattern::Teleport:
    {
        StopMovement();

        // 텔레포트 시도 (타겟 뒤 150~200 거리)
        bool bTeleportSuccess = MyPawn->TryTeleportToTarget(TargetActor, 200.0f);

        if (bTeleportSuccess)
        {
            UE_LOG(LogTemp, Warning, TEXT("MageAI: Teleport Success! -> IMMEDIATE ATTACK!"));

            // [핵심 수정] 
            // 기존: CurrentPattern = EStage2Pattern::MeleeChase; (추격함)
            // 변경: 묻지도 따지지도 않고 바로 공격 패턴으로 직행!
            CurrentPattern = EStage2Pattern::MeleeAttack;

            // 텔레포트 연출 후 딜레이 없이 바로 때리려면 0.0f
            // 나타나는 연출(짠!)을 조금 보여주고 때리려면 0.2f ~ 0.5f
            PatternCooldown = 0.2f;
        }
        else
        {
            // 텔레포트 실패 시에는 어쩔 수 없이 걸어가야 함
            CurrentPattern = EStage2Pattern::MeleeChase;
        }
    }
    break;

    case EStage2Pattern::MeleeChase:
        // (텔레포트 실패했을 때만 여기로 옴 -> 걸어서 추격)
    {
        float Dist = MyPawn->GetDistanceTo(TargetActor);
        if (Dist <= 200.0f)
        {
            StopMovement();
            CurrentPattern = EStage2Pattern::MeleeAttack;
        }
        else
        {
            MoveToActor(TargetActor, 50.0f);
        }
    }
    break;

    case EStage2Pattern::MeleeAttack:
    {
        StopMovement();

        // [수정] 텔레포트 직후라서 회전이 안 맞을 수 있으니, 
        // 공격 직전에 타겟 쪽으로 몸을 확 돌려버림 (Aim Snap)
        if (TargetActor)
        {
            FVector Dir = TargetActor->GetActorLocation() - MyPawn->GetActorLocation();
            Dir.Z = 0.f;
            MyPawn->SetActorRotation(Dir.Rotation());
        }

        // 지팡이 휘두르기 (공격 범위 체크는 PerformAttackTrace에서 함)
        UE_LOG(LogTemp, Warning, TEXT("MageAI: Melee Finisher!"));
        float AnimTime = MyPawn->PlayMeleeAttackAnim();

        float WaitTime = (AnimTime > 0.f) ? AnimTime : 1.5f;
        GetWorld()->GetTimerManager().SetTimer(StateTimerHandle, this, &AMidBossAIController::FinishAttack, WaitTime, false);

        UpdateRandomTarget();

        CurrentPattern = EStage2Pattern::MagicBarrage;
        MagicFireCount = 0;
        PatternCooldown = 2.0f;
    }
    break;
    }
}