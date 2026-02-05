// Source/Project_Bang_Squad/Character/MonsterBase/MidBossAIController.cpp

#include "MidBossAIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h"
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"

AMidBossAIController::AMidBossAIController()
{
    // 1. Perception 생성
    SetPerceptionComponent(*CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AI Perception")));

    // 2. 시야 설정
    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("Sight Config"));
    if (SightConfig)
    {
        SightConfig->SightRadius = 1500.0f;
        SightConfig->LoseSightRadius = 2000.0f;
        SightConfig->PeripheralVisionAngleDegrees = 180.0f; // 시야각 (필요 시 360도로 확장)

        // [중요] 모든 진영 감지 (플레이어는 보통 Neutral 또는 Different Team)
        SightConfig->DetectionByAffiliation.bDetectEnemies = true;
        SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
        SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

        GetPerceptionComponent()->ConfigureSense(*SightConfig);
        GetPerceptionComponent()->SetDominantSense(SightConfig->GetSenseImplementation());
    }

    CurrentAIState = EMidBossAIState::Idle;
    PrimaryActorTick.bCanEverTick = true;
    bHasRoared = false;
}

void AMidBossAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    bHasRoared = false;

    // Perception 이벤트 연결
    if (GetPerceptionComponent())
    {
        GetPerceptionComponent()->OnTargetPerceptionUpdated.AddDynamic(this, &AMidBossAIController::OnTargetDetected);
    }

    // 랜덤 타겟 변경 타이머 시작
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            AggroTimerHandle,
            this,
            &AMidBossAIController::UpdateRandomTarget,
            TargetChangeInterval,
            true
        );
    }
}

void AMidBossAIController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // [예외 처리] 사망했거나 기믹 중이면 로직 정지
    if (CurrentAIState == EMidBossAIState::Dead || CurrentAIState == EMidBossAIState::Gimmick)
    {
        return;
    }

    // [타겟 검증] 타겟이 사라지거나 죽었으면 초기화
    if (CurrentAIState == EMidBossAIState::Chase || CurrentAIState == EMidBossAIState::Attack)
    {
        if (!IsValid(TargetActor) || IsTargetDead(TargetActor))
        {
            TargetActor = nullptr;
            CurrentAIState = EMidBossAIState::Idle;
            StopMovement();
            return;
        }
    }

    // [FSM 실행] 상태별로 가상 함수 호출 -> 자식 클래스에서 구체적 행동 정의
    switch (CurrentAIState)
    {
    case EMidBossAIState::Idle:
        UpdateIdleState(DeltaTime);
        break;
    case EMidBossAIState::Notice:
        // 포효 중 (타이머가 끝나면 StartChasing 호출)
        break;
    case EMidBossAIState::Chase:
        UpdateChaseState(DeltaTime);
        break;
    case EMidBossAIState::Attack:
        UpdateAttackState(DeltaTime);
        break;
    case EMidBossAIState::Hit:
        // 피격 경직 중 (타이머가 끝나면 StartChasing 호출)
        break;
    case EMidBossAIState::Dead:
        break;
    }
}

// --- [FSM Virtual Implementations (Default)] ---

void AMidBossAIController::UpdateIdleState(float DeltaTime)
{
    // 기본 로직: 타겟이 있으면 바로 추격
    if (TargetActor)
    {
        StartChasing();
    }
}

void AMidBossAIController::UpdateChaseState(float DeltaTime)
{
    // 기본 로직: 타겟을 향해 단순 이동
    if (TargetActor)
    {
        MoveToActor(TargetActor, AttackRange * 0.8f);
    }
}

void AMidBossAIController::UpdateAttackState(float DeltaTime)
{
    // 부모 클래스는 공격 방식(칼? 마법?)을 모르므로 비워둡니다.
    // 자식 클래스(Stage1AI, Stage2AI)에서 반드시 오버라이드 해야 합니다.
}

// --- [Helper Functions] ---

void AMidBossAIController::OnTargetDetected(AActor* Actor, FAIStimulus Stimulus)
{
    if (CurrentAIState == EMidBossAIState::Dead) return;

    auto* PlayerCharacter = Cast<ABaseCharacter>(Actor);
    if (!PlayerCharacter || !Stimulus.WasSuccessfullySensed() || IsTargetDead(PlayerCharacter))
    {
        return;
    }

    // 최초 발견 시 로직
    if (CurrentAIState == EMidBossAIState::Idle)
    {
        TargetActor = PlayerCharacter;

        // 아직 포효 안 했으면 포효
        if (!bHasRoared)
        {
            bHasRoared = true;
            CurrentAIState = EMidBossAIState::Notice;
            StopMovement();

            // 몽타주 재생은 폰에게 위임 (EnemyCharacterBase에 PlayAggroAnim이 있다고 가정)
            float WaitTime = 1.5f;
            if (auto* MyPawn = Cast<AEnemyCharacterBase>(GetPawn()))
            {
                // 부모 캐릭터 클래스에 가상함수로 PlayAggroAnim()이 있다면 호출, 없으면 시간만 대기
                // WaitTime = MyPawn->PlayAggroAnim(); 
            }

            GetWorld()->GetTimerManager().SetTimer(StateTimerHandle, this, &AMidBossAIController::StartChasing, WaitTime, false);
        }
        else
        {
            StartChasing();
        }
    }
}

void AMidBossAIController::OnDamaged(AActor* Attacker)
{
    if (CurrentAIState == EMidBossAIState::Dead || CurrentAIState == EMidBossAIState::Gimmick) return;

    GetWorld()->GetTimerManager().ClearTimer(StateTimerHandle);
    StopMovement();

    // 1. 공격자 식별 (Owner 확인)
    AActor* RealTarget = Attacker;
    if (Attacker)
    {
        if (!RealTarget->IsA(APawn::StaticClass()))
        {
            if (Attacker->GetInstigator()) RealTarget = Attacker->GetInstigator();
            else if (Attacker->GetOwner()) RealTarget = Attacker->GetOwner();
        }
    }

    // 2. 아군 오인 사격 방지
    bool bIsEnemyTeam = RealTarget && RealTarget->IsA(AEnemyCharacterBase::StaticClass());

    if (RealTarget && !IsTargetDead(RealTarget) && !bIsEnemyTeam)
    {
        // 타겟 변경 (어그로 핑퐁)
        TargetActor = RealTarget;
        if (!bHasRoared) bHasRoared = true;
    }

    // 3. 피격 상태 전환
    CurrentAIState = EMidBossAIState::Hit;
    float StunDuration = 0.5f;

    // 피격 애니메이션 재생 요청 (폰에게 위임)
    /* [설계 포인트]
       모든 적 캐릭터가 공통으로 PlayHitReactAnim을 가진다면 AEnemyCharacterBase에 가상함수로 넣는 것이 좋습니다.
       여기서는 일단 타이머만 설정합니다.
    */

    GetWorld()->GetTimerManager().SetTimer(StateTimerHandle, this, &AMidBossAIController::StartChasing, StunDuration, false);
}

void AMidBossAIController::UpdateRandomTarget()
{
    if (CurrentAIState == EMidBossAIState::Dead || CurrentAIState == EMidBossAIState::Gimmick) return;

    TArray<AActor*> PerceivedActors;
    GetPerceptionComponent()->GetKnownPerceivedActors(UAISenseConfig_Sight::StaticClass(), PerceivedActors);

    TArray<AActor*> ValidTargets;
    for (AActor* Actor : PerceivedActors)
    {
        ABaseCharacter* PlayerChar = Cast<ABaseCharacter>(Actor);
        if (PlayerChar && IsValid(PlayerChar) && !IsTargetDead(PlayerChar))
        {
            ValidTargets.Add(Actor);
        }
    }

    if (ValidTargets.Num() > 0)
    {
        int32 RandomIndex = FMath::RandRange(0, ValidTargets.Num() - 1);
        AActor* NewTarget = ValidTargets[RandomIndex];

        if (TargetActor != NewTarget)
        {
            TargetActor = NewTarget;
            // 타겟 변경 시 잠깐 멈춤 or 즉시 반응 (기획에 따라 조정)
            // StopMovement(); 
            UE_LOG(LogTemp, Log, TEXT("AI: Switched Target to %s"), *TargetActor->GetName());
        }
    }
}

void AMidBossAIController::StartChasing()
{
    if (CurrentAIState == EMidBossAIState::Dead) return;

    CurrentAIState = EMidBossAIState::Chase;

    // Tick에서 UpdateChaseState가 호출되면서 이동 시작함
}

void AMidBossAIController::FinishAttack()
{
    if (CurrentAIState == EMidBossAIState::Dead) return;

    // 공격이 끝나면 다시 추격 상태로 복귀
    CurrentAIState = EMidBossAIState::Chase;
}

void AMidBossAIController::SetDeadState()
{
    CurrentAIState = EMidBossAIState::Dead;
    StopMovement();

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(StateTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(AggroTimerHandle);
    }

    // 감각 비활성화
    GetPerceptionComponent()->SetSenseEnabled(UAISense_Sight::StaticClass(), false);
}

bool AMidBossAIController::IsTargetDead(AActor* Actor) const
{
    if (!Actor) return true;

    if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(Actor))
    {
        if (BaseChar->IsDead()) return true;
    }

    UHealthComponent* HP = Actor->FindComponentByClass<UHealthComponent>();
    if (HP && HP->IsDead()) return true;

    return false;
}