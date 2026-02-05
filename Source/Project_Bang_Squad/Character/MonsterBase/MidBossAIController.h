// Source/Project_Bang_Squad/Character/MonsterBase/MidBossAIController.h

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "MidBossAIController.generated.h"

class UAISenseConfig_Sight;

// FSM 상태 정의
UENUM(BlueprintType)
enum class EMidBossAIState : uint8
{
    Idle        UMETA(DisplayName = "Idle"),
    Notice      UMETA(DisplayName = "Notice (Roar)"),
    Chase       UMETA(DisplayName = "Chase"),
    Attack      UMETA(DisplayName = "Attack"),
    Hit         UMETA(DisplayName = "Hit (Stunned)"),
    Gimmick     UMETA(DisplayName = "Gimmick"),
    Dead        UMETA(DisplayName = "Dead")
};

UCLASS()
class PROJECT_BANG_SQUAD_API AMidBossAIController : public AAIController
{
    GENERATED_BODY()

public:
    AMidBossAIController();

    virtual void OnPossess(APawn* InPawn) override;
    virtual void Tick(float DeltaTime) override;

    // --- [Public Interface] ---

    // 피격 시 호출 (외부에서 호출 가능)
    void OnDamaged(AActor* Attacker);

    // 사망 시 AI 정지
    void SetDeadState();

    // 공격 사거리 설정
    void SetAttackRange(float NewRange) { AttackRange = NewRange; }

    // 공격 종료 시 호출 (타이머 or 몽타주 종료 시)
    UFUNCTION()
    virtual void FinishAttack();

protected:
    // --- [FSM Virtual Functions] ---
    // 자식 클래스(Stage1, Stage2)가 오버라이드하여 각자의 행동을 구현
    virtual void UpdateIdleState(float DeltaTime);
    virtual void UpdateChaseState(float DeltaTime);
    virtual void UpdateAttackState(float DeltaTime);

    // --- [Helper Functions] ---

    // Perception 감지 시
    UFUNCTION()
    void OnTargetDetected(AActor* Actor, FAIStimulus Stimulus);

    // 랜덤 타겟 변경 (멀티플레이어 어그로 분산)
    virtual void UpdateRandomTarget();

    // 타겟 사망 여부 확인
    bool IsTargetDead(AActor* Actor) const;

    // 추격 시작 헬퍼
    virtual void StartChasing();

protected:
    // --- [Components] ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    TObjectPtr<UAISenseConfig_Sight> SightConfig;

    // --- [State] ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|State")
    EMidBossAIState CurrentAIState;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|State")
    TObjectPtr<AActor> TargetActor;

    // --- [Timers & Config] ---
    FTimerHandle StateTimerHandle;
    FTimerHandle AggroTimerHandle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Config")
    float AttackRange = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Config")
    float TargetChangeInterval = 5.0f;

    // 한 번이라도 포효했는지 체크
    bool bHasRoared = false;
};