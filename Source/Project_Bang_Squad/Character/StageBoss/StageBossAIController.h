// Source/Project_Bang_Squad/Character/StageBoss/StageBossAIController.h
#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "StageBossAIController.generated.h"

// 보스 AI 상태 정의
enum class EBossAIState : uint8
{
    Idle,           // 대기
    Chase,          // 추적
    MeleeAttack,    // 근접 공격 (3회)
    Retreat,        // 후퇴 (플레이어 반대 방향)
    RangeAttack,    // 원거리 공격 (3회)
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
    EBossAIState CurrentState = EBossAIState::Idle;
    void SetState(EBossAIState NewState);

    void HandleIdle(float DeltaTime);
    void HandleChase(float DeltaTime);
    void HandleMeleeAttack(float DeltaTime);
    void HandleRetreat(float DeltaTime);
    void HandleRangeAttack(float DeltaTime);

    bool IsTargetValid() const;
    FVector GetRetreatLocation() const;

    UPROPERTY()
    TObjectPtr<class AStage1Boss> OwnerBoss;

    UPROPERTY(VisibleAnywhere, Category = "Boss AI|Debug")
    TObjectPtr<AActor> TargetActor;

    int32 CurrentAttackCount = 0;
    float StateTimer = 0.0f;
    float AttackCooldownTimer = 0.0f;

    // ==============================================================================
    // [설정] 이제 에디터(블루프린트)에서 이 값들을 마음대로 조절하세요!
    // ==============================================================================
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Config|Movement")
    float AttackTriggerRange = 500.0f; // 공격 시작 거리 (캡슐이 커졌으니 넉넉하게)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Config|Movement")
    float MoveStopRange = 10.0f;       // 이동 멈춤 거리 (바짝 붙게)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Config|Retreat")
    float RetreatDuration = 2.0f;      // 후퇴 시간

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Config|Retreat")
    float RetreatDistance = 800.0f;    // 후퇴 거리

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Config|Combat")
    int32 MaxComboCount = 3;           // 3회 공격

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss AI|Config|Combat")
    float AttackInterval = 1.5f;       // 공격 간 딜레이 (초)
};