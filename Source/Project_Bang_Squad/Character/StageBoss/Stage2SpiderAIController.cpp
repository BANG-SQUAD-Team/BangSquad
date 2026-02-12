// Source/Project_Bang_Squad/Character/StageBoss/Stage2SpiderAIController.cpp

#include "Project_Bang_Squad/Character/StageBoss/Stage2SpiderAIController.h"
#include "Project_Bang_Squad/Character/StageBoss/Stage2Boss.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBossData.h"


AStage2SpiderAIController::AStage2SpiderAIController()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AStage2SpiderAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    SpiderBoss = Cast<AStage2Boss>(InPawn);

    CurrentState = ESpiderPatternState::Chase;
    PatternCycleIndex = 0;
    FindNearestTarget();
}

void AStage2SpiderAIController::OnUnPossess()
{
    SpiderBoss = nullptr;
    Super::OnUnPossess();
}

void AStage2SpiderAIController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!SpiderBoss || SpiderBoss->IsDead()) return;

    if (SpiderBoss->IsInvinciblePhase())
    {
        StopMovement();
        return;
    }

    // [Fix: IsValid 전역 함수 사용]
    if (!IsValid(TargetActor))
    {
        FindNearestTarget();
        if (!IsValid(TargetActor)) return;
    }

    switch (CurrentState)
    {
    case ESpiderPatternState::Chase:
        UpdateChase(DeltaTime);
        break;

    case ESpiderPatternState::Retreat:
        UpdateRetreat(DeltaTime);
        break;

    case ESpiderPatternState::SmashQTE:
    {
        // StateTimer가 -1.0f면 "아직 도착 안 함(돌진 중)" 상태로 간주
        if (StateTimer < 0.0f)
        {
            // 1. 타겟에게 맹렬히 다시 접근
            MoveToActor(TargetActor, 1.0f);

            // 2. 공격 사거리 체크 (UpdateChase와 동일 로직)
            float WeaponReach = 150.0f;
            if (SpiderBoss && SpiderBoss->GetBossData())
                WeaponReach = SpiderBoss->GetBossData()->AttackRange;

            float BossRadius = SpiderBoss->GetCapsuleComponent()->GetScaledCapsuleRadius();
            float Dist = FVector::Dist(SpiderBoss->GetActorLocation(), TargetActor->GetActorLocation());

            // 표면 거리가 사거리 안쪽이면? -> 찍기 발동!
            // (더 바짝 붙게 하려고 -50.0f 함)
            if (Dist <= (BossRadius + WeaponReach - 50.0f))
            {
                StopMovement();
                SpiderBoss->PerformSmashAttack(TargetActor); // 쾅!
                StateTimer = 3.0f; // 공격 후 3초간 대기 (후딜레이)
            }
        }
        // StateTimer가 양수면 "이미 찍었고 쉬는 중"
        else
        {
            StateTimer -= DeltaTime;
            if (StateTimer <= 0.0f)
            {
                FindNearestTarget();
                CurrentState = ESpiderPatternState::Chase; // 다시 평타 추격 모드로
            }
        }
    }
    break;

    // 나머지 패턴(평타, 거미줄)은 기존 타이머 방식 유지
    case ESpiderPatternState::Attack:
    case ESpiderPatternState::WebShot:
        if (StateTimer > 0.0f)
        {
            StateTimer -= DeltaTime;
        }
        else
        {
            if (CurrentState == ESpiderPatternState::Attack)
            {
                // 평타 때리고 나면 -> 후퇴(Retreat) 시작
                CurrentState = ESpiderPatternState::Retreat;
                StateTimer = 2.0f; // 2초 동안 뒤로 도망감

                FRotator BackRot = SpiderBoss->GetActorRotation();
                BackRot.Yaw += 180.0f;
                SpiderBoss->SetActorRotation(BackRot);
            }
            else // WebShot 끝남
            {
                FindNearestTarget();
                CurrentState = ESpiderPatternState::Chase;
            }
        }
        break;
    }
}

void AStage2SpiderAIController::UpdateChase(float DeltaTime)
{
    if (!IsValid(TargetActor)) return;

    // 사거리 데이터 가져오기
    float BaseRange = 150.0f;
    if (SpiderBoss && SpiderBoss->GetBossData())
        BaseRange = SpiderBoss->GetBossData()->AttackRange;

    // 덩치 가져오기
    float BossRadius = 0.0f;
    if (SpiderBoss && SpiderBoss->GetCapsuleComponent())
        BossRadius = SpiderBoss->GetCapsuleComponent()->GetScaledCapsuleRadius();

    float TargetRadius = 40.0f;
    if (TargetActor->FindComponentByClass<UCapsuleComponent>())
        TargetRadius = TargetActor->FindComponentByClass<UCapsuleComponent>()->GetScaledCapsuleRadius();

    // 표면 거리 계산
    float CenterDist = FVector::Dist(SpiderBoss->GetActorLocation(), TargetActor->GetActorLocation());
    float SurfaceDist = CenterDist - BossRadius - TargetRadius;

    // 바짝 붙어서(-10) 평타 때리기
    if (SurfaceDist <= (BaseRange - 10.0f))
    {
        StopMovement();
        CurrentState = ESpiderPatternState::Attack;

        float AnimTime = SpiderBoss->PlayMeleeAttackAnim();
        StateTimer = (AnimTime > 0) ? AnimTime : 1.0f;
    }
    else
    {
        MoveToActor(TargetActor, 1.0f);
    }
}

void AStage2SpiderAIController::UpdateRetreat(float DeltaTime)
{
    // 후퇴 중...
    if (StateTimer > 0.0f)
    {
        StateTimer -= DeltaTime;

        // [수정] 복잡한 계산 다 필요 없음!
        // 그냥 뒤(Forward)로 가라고 입력만 넣으면 됩니다.
        // 낭떨어지면? CharacterMovement가 알아서 멈춰세움 (bCanWalkOffLedges = false 덕분)
        FVector Forward = SpiderBoss->GetActorForwardVector();
        SpiderBoss->AddMovementInput(Forward, 1.0f);
    }
    // 후퇴 끝
    else
    {
        StopMovement();
        FindNearestTarget();

        // 패턴 전환 (기존과 동일)
        if (PatternCycleIndex % 2 == 0)
        {
            CurrentState = ESpiderPatternState::WebShot;
            SpiderBoss->PerformWebShot(TargetActor);
            StateTimer = 2.0f;
        }
        else
        {
            CurrentState = ESpiderPatternState::SmashQTE;
            StateTimer = -1.0f;
        }
        PatternCycleIndex++;
    }
}

bool AStage2SpiderAIController::IsSafeToRetreat(FVector Direction, float CheckDistance)
{
    if (!SpiderBoss) return false;

    FVector Start = SpiderBoss->GetActorLocation();
    FVector CheckPos = Start + (Direction * CheckDistance);

    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!NavSys) return true;

    FNavLocation Result;
    bool bOnNavMesh = NavSys->ProjectPointToNavigation(CheckPos, Result, FVector(500.0f, 500.0f, 500.0f));

    return bOnNavMesh;
}

void AStage2SpiderAIController::FindNearestTarget()
{
    TArray<AActor*> Candidates;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), Candidates);

    TArray<AActor*> ValidPlayers;

    for (AActor* Actor : Candidates)
    {
        // 1. 나 자신은 제외
        if (Actor == SpiderBoss) continue;

        // 2. 같은 몬스터 진영 제외 (EnemyCharacterBase 상속 여부로 판단)
        // (EnemyCharacterBase 헤더가 필요할 수 있음. 없으면 태그나 클래스 이름으로 체크)
        if (Actor->ActorHasTag(TEXT("Enemy"))) continue;

        // 3. 죽은 플레이어 제외 (선택사항)
         //if (Change to your specific player class -> IsDead()) continue;

        ValidPlayers.Add(Actor);
    }

    if (ValidPlayers.Num() > 0)
    {
        // 랜덤 타겟 (또는 가장 가까운 타겟 로직 추가 가능)
        int32 Idx = FMath::RandRange(0, ValidPlayers.Num() - 1);
        TargetActor = ValidPlayers[Idx];
    }
}

void AStage2SpiderAIController::StartPhasePattern()
{
    StopMovement();
    CurrentState = ESpiderPatternState::PhaseWait;
}

void AStage2SpiderAIController::EndPhasePattern()
{
    FindNearestTarget();
    CurrentState = ESpiderPatternState::Chase;
}