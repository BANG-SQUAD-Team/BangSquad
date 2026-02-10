// Source/Project_Bang_Squad/Character/StageBoss/Stage2SpiderAIController.cpp

#include "Project_Bang_Squad/Character/StageBoss/Stage2SpiderAIController.h"
#include "Project_Bang_Squad/Character/StageBoss/Stage2Boss.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"

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

    case ESpiderPatternState::Attack:
    case ESpiderPatternState::WebShot:
    case ESpiderPatternState::SmashQTE:
        if (StateTimer > 0.0f)
        {
            StateTimer -= DeltaTime;
        }
        else
        {
            if (CurrentState == ESpiderPatternState::Attack)
            {
                CurrentState = ESpiderPatternState::Retreat;
                StateTimer = 2.0f;

                FRotator BackRot = SpiderBoss->GetActorRotation();
                BackRot.Yaw += 180.0f;
                SpiderBoss->SetActorRotation(BackRot);
            }
            else
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

    // [Fix: .Get() 제거]
    float Dist = FVector::Dist(SpiderBoss->GetActorLocation(), TargetActor->GetActorLocation());
    float AttackRange = 250.0f;

    if (Dist <= AttackRange)
    {
        StopMovement();
        CurrentState = ESpiderPatternState::Attack;

        float AnimTime = SpiderBoss->PlayMeleeAttackAnim();
        StateTimer = (AnimTime > 0) ? AnimTime : 1.0f;
    }
    else
    {
        MoveToActor(TargetActor, AttackRange - 50.0f);
    }
}

void AStage2SpiderAIController::UpdateRetreat(float DeltaTime)
{
    if (StateTimer > 0.0f)
    {
        StateTimer -= DeltaTime;
        FVector Forward = SpiderBoss->GetActorForwardVector();

        if (IsSafeToRetreat(Forward, 300.0f))
        {
            MoveToLocation(SpiderBoss->GetActorLocation() + Forward * 500.0f);
        }
        else
        {
            StopMovement();
            StateTimer = 0.0f;
        }
    }
    else
    {
        StopMovement();
        FindNearestTarget();

        if (PatternCycleIndex % 2 == 0)
        {
            CurrentState = ESpiderPatternState::WebShot;
            SpiderBoss->PerformWebShot(TargetActor);
            StateTimer = 2.0f;
        }
        else
        {
            CurrentState = ESpiderPatternState::SmashQTE;
            SpiderBoss->PerformSmashAttack(TargetActor);
            StateTimer = 3.0f;
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
    bool bOnNavMesh = NavSys->ProjectPointToNavigation(CheckPos, Result, FVector(50, 50, 200));

    return bOnNavMesh;
}

void AStage2SpiderAIController::FindNearestTarget()
{
    TArray<AActor*> Players;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), Players);

    if (Players.Num() > 0)
    {
        int32 Idx = FMath::RandRange(0, Players.Num() - 1);
        TargetActor = Players[Idx];
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