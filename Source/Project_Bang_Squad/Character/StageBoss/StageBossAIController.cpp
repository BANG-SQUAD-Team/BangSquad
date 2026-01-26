// Source/Project_Bang_Squad/Character/StageBoss/StageBossAIController.cpp
#include "StageBossAIController.h"
#include "Stage1Boss.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "GameFramework/Character.h"
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"
#include "DrawDebugHelpers.h"
#include "Components/CapsuleComponent.h"

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

    // 왜 이렇게 짰는가: 보스가 몽타주 재생 중이면 AI의 모든 판단 로직을 잠시 중단합니다.
    // 하지만 상태(CurrentState)는 이미 바뀌어 있을 수 있습니다.
    if (OwnerBoss->bIsActionInProgress) return;

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
    case EBossAIState::Idle:         HandleIdle(DeltaTime); break;
    case EBossAIState::Chase:        HandleChase(DeltaTime); break;
    case EBossAIState::MeleeAttack:  HandleMeleeAttack(DeltaTime); break;
    case EBossAIState::Retreat:      HandleRetreat(DeltaTime); break;
    case EBossAIState::RangeAttack:  HandleRangeAttack(DeltaTime); break;
    case EBossAIState::SpikeAttack:  HandleSpikeAttack(DeltaTime); break;
    case EBossAIState::SwitchTarget: HandleSwitchTarget(); break;
    }
}

void AStageBossAIController::SetState(EBossAIState NewState)
{
    // [중요] 상태 변경은 언제든 허용합니다. (잠긴 상태에서 SetState 호출을 무시하지 않음)
    CurrentState = NewState;
    StateTimer = 0.0f;
    AttackCooldownTimer = 0.0f;

    switch (CurrentState)
    {
    case EBossAIState::Chase:
        if (TargetActor) SetFocus(TargetActor);
        break;

    case EBossAIState::Retreat:
        ClearFocus(EAIFocusPriority::Gameplay);
        StopMovement();
        MoveToLocation(GetStraightRetreatLocation(), 50.0f, true, true, true, true, 0, true);
        break;

    case EBossAIState::RangeAttack:
        StopMovement();
        if (TargetActor) SetFocus(TargetActor);
        CurrentAttackCount = 0;
        AttackCooldownTimer = 0.5f;
        break;

    case EBossAIState::MeleeAttack:
        StopMovement();
        if (TargetActor) SetFocus(TargetActor);
        CurrentAttackCount = 0;
        break;

    case EBossAIState::SpikeAttack:
        StopMovement();
        if (TargetActor) SetFocus(TargetActor);
        // Spike 패턴 진입 시 몽타주 실행은 Character 쪽 함수에서 Action Lock을 겁니다.
        if (OwnerBoss) OwnerBoss->StartSpikePattern();
        break;
    }
}

void AStageBossAIController::HandleMeleeAttack(float DeltaTime)
{
    if (AttackCooldownTimer > 0.0f)
    {
        AttackCooldownTimer -= DeltaTime;
        return;
    }

    // 콤보 횟수 체크
    if (CurrentAttackCount >= MaxComboCount)
    {
        SetState(EBossAIState::Retreat);
        return;
    }

    // 왜 이렇게 짰는가: 보스가 이전 공격의 몽타주를 아직 재생 중이라면 다음 공격 명령을 유보합니다.
    if (OwnerBoss->bIsActionInProgress) return;

    OwnerBoss->DoAttack_Swing();
    CurrentAttackCount++;
    AttackCooldownTimer = AttackInterval;
}

void AStageBossAIController::HandleRangeAttack(float DeltaTime)
{
    if (TargetActor) SetFocus(TargetActor);
    if (AttackCooldownTimer > 0.0f)
    {
        AttackCooldownTimer -= DeltaTime;
        return;
    }

    if (CurrentAttackCount >= MaxComboCount)
    {
        SetState(EBossAIState::SpikeAttack);
        return;
    }

    if (OwnerBoss->bIsActionInProgress) return;

    OwnerBoss->DoAttack_Slash();
    CurrentAttackCount++;
    AttackCooldownTimer = AttackInterval;
}

void AStageBossAIController::HandleSwitchTarget()
{
    TArray<AActor*> Candidates;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), Candidates);
    TArray<AActor*> ValidPlayers;

    for (AActor* Actor : Candidates)
    {
        ACharacter* Char = Cast<ACharacter>(Actor);
        if (Char && Char != OwnerBoss && Char->IsPlayerControlled())
        {
            UHealthComponent* HC = Char->FindComponentByClass<UHealthComponent>();
            if (HC && !HC->IsDead()) ValidPlayers.Add(Char);
        }
    }

    if (ValidPlayers.Num() == 0) SetState(EBossAIState::Idle);
    else
    {
        if (ValidPlayers.Num() > 1 && TargetActor) ValidPlayers.Remove(TargetActor);
        TargetActor = ValidPlayers[FMath::RandRange(0, ValidPlayers.Num() - 1)];
        SetState(EBossAIState::Chase);
    }
}

void AStageBossAIController::HandleIdle(float DeltaTime)
{
    TargetActor = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (IsTargetValid()) SetState(EBossAIState::Chase);
}

void AStageBossAIController::HandleChase(float DeltaTime)
{
    if (!IsTargetValid()) return;
    float Dist2D = FVector::DistXY(OwnerBoss->GetActorLocation(), TargetActor->GetActorLocation());
    if (Dist2D <= AttackTriggerRange)
    {
        StopMovement();
        SetState(EBossAIState::MeleeAttack);
    }
    else MoveToActor(TargetActor, MoveStopRange);
}

void AStageBossAIController::HandleRetreat(float DeltaTime)
{
    StateTimer += DeltaTime;
    if (StateTimer >= RetreatDuration) SetState(EBossAIState::RangeAttack);
}

void AStageBossAIController::HandleSpikeAttack(float DeltaTime)
{
    StateTimer += DeltaTime;
    if (StateTimer >= SpikePatternDuration) SetState(EBossAIState::SwitchTarget);
}

FVector AStageBossAIController::GetStraightRetreatLocation() const
{
    if (!OwnerBoss) return FVector::ZeroVector;
    FVector Origin = OwnerBoss->GetActorLocation();
    FVector BackwardDir = -OwnerBoss->GetActorForwardVector();
    FHitResult Hit;
    FCollisionQueryParams P; P.AddIgnoredActor(OwnerBoss);
    FVector Dest = GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + (BackwardDir * RetreatDistance), ECC_WorldStatic, P)
        ? Hit.Location + (Hit.Normal * (OwnerBoss->GetCapsuleComponent()->GetScaledCapsuleRadius() + 100.f))
        : Origin + (BackwardDir * RetreatDistance);
    FNavLocation NavLoc;
    return (UNavigationSystemV1::GetCurrent(GetWorld())->ProjectPointToNavigation(Dest, NavLoc, FVector(500, 500, 500))) ? NavLoc.Location : Origin;
}

bool AStageBossAIController::IsTargetValid() const { return (TargetActor && IsValid(TargetActor)); }