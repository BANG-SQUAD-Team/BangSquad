// Source/Project_Bang_Squad/Character/StageBoss/Stage2SpiderAIController.h

#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/StageBoss/StageBossAIController.h"
#include "Stage2SpiderAIController.generated.h"

class AStage2Boss;

// [패턴 상태 정의]
UENUM(BlueprintType)
enum class ESpiderPatternState : uint8
{
    Chase,          // 1. 추격
    Attack,         // 2. 기본 공격 (평타)
    Retreat,        // 3. 후퇴 (낭떨어지 체크)
    WebShot,        // 4. 거미줄 발사 (패턴 A)
    SmashQTE,       // 5. 내려찍기 & QTE (패턴 B)
    PhaseWait       // 6. 소환 페이즈 (무적 대기)
};

/**
 * Stage 2 거미 보스 전용 AI
 * 특징: 추격-공격-후퇴-특수패턴을 반복하며, 체력 트리거에 따라 페이즈 전환
 */
UCLASS()
class PROJECT_BANG_SQUAD_API AStage2SpiderAIController : public AStageBossAIController
{
    GENERATED_BODY()

public:
    AStage2SpiderAIController();

    virtual void OnPossess(APawn* InPawn) override;
    virtual void Tick(float DeltaTime) override;
    virtual void OnUnPossess() override;

    // [페이즈 제어] 보스가 "나 체력 70%야!" 라고 알리면 호출됨
    void StartPhasePattern();
    void EndPhasePattern();

protected:
    // --- [FSM Logic] ---
    void UpdateChase(float DeltaTime);
    void UpdateRetreat(float DeltaTime);
    void UpdatePatternWait(float DeltaTime); // 공격 후 대기 등

    // --- [Helper] ---
    void FindNearestTarget();
    bool IsSafeToRetreat(FVector Direction, float CheckDistance); // 낭떨어지 체크

private:
    UPROPERTY()
    TObjectPtr<AStage2Boss> SpiderBoss;

    // UPROPERTY()
    // TWeakObjectPtr<AActor> TargetActor;

     // 현재 상태
    ESpiderPatternState CurrentState;

    // 패턴 사이클 인덱스 (0:거미줄, 1:스매시 반복용)
    int32 PatternCycleIndex = 0;

    // 타이머 및 상태 변수
    float StateTimer = 0.0f;
    FVector RetreatLocation;
    bool bHasAttacked = false;
    bool bIsDoingFollowUp = false; // 추가타 진행 중인지 체크하는 플래그
};