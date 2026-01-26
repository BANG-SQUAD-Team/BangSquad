// Source/Project_Bang_Squad/Character/StageBoss/StageBossAIController.h
#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "StageBossAIController.generated.h"

/** 보스 AI 상태 정의 */
UENUM(BlueprintType)
enum class EBossAIState : uint8
{
    Idle,           // 대기
    Chase,          // 추적
    MeleeAttack,    // 근접 공격 (3회)
    Retreat,        // 후퇴 (직선 도주)
    RangeAttack,    // 원거리 공격 (3회)
    SpikeAttack,    // 스파이크 패턴 (원거리 공격 후 연계)
    SwitchTarget    // 타겟 변경
};

UCLASS()
class PROJECT_BANG_SQUAD_API AStageBossAIController : public AAIController
{
    GENERATED_BODY()

public:
    AStageBossAIController();

    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void Tick(float DeltaTime) override;

protected:
    // --- [State Machine] ---
    UPROPERTY(VisibleAnywhere, Category = "Boss AI|State")
    EBossAIState CurrentState = EBossAIState::Idle;

    // 왜 이렇게 짰는가: 상태 변경은 언제든 가능해야 하지만, 
    // 실제 행동은 보스의 애니메이션(ActionInProgress) 종료 여부에 따라 결정됩니다.
    void SetState(EBossAIState NewState);

    void HandleIdle(float DeltaTime);
    void HandleChase(float DeltaTime);
    void HandleMeleeAttack(float DeltaTime);
    void HandleRetreat(float DeltaTime);
    void HandleRangeAttack(float DeltaTime);
    void HandleSpikeAttack(float DeltaTime);
    void HandleSwitchTarget(); // 상태 로직 통합

    bool IsTargetValid() const;

    // 등 뒤로 직선 후퇴 좌표 계산
    FVector GetStraightRetreatLocation() const;

    UPROPERTY()
    TObjectPtr<class AStage1Boss> OwnerBoss;

    UPROPERTY(VisibleAnywhere, Category = "Boss AI|Debug")
    TObjectPtr<AActor> TargetActor;

    int32 CurrentAttackCount = 0;
    float StateTimer = 0.0f;
    float AttackCooldownTimer = 0.0f;

public:
    // --- [Config] ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Config|Movement")
    float AttackTriggerRange = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Config|Movement")
    float MoveStopRange = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Config|Retreat")
    float RetreatDuration = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Config|Retreat")
    float RetreatDistance = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Config|Combat")
    int32 MaxComboCount = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Config|Combat")
    float AttackInterval = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Config|Combat")
    float SpikePatternDuration = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Debug")
    bool bShowDebugLines = true;
};