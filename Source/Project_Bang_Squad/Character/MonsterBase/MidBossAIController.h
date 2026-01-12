// MidBossAIController.h

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
	virtual void Tick(float DeltaTime) override;

	// 캐릭터가 피격당했을 때 호출하는 함수
	void OnDamaged(AActor* Attacker);

	// [필수 추가] 사망 시 AI 정지 명령 함수
	void SetDeadState();

protected:
	virtual void OnPossess(APawn* InPawn) override;

	UFUNCTION()
	void OnTargetDetected(AActor* Actor, FAIStimulus Stimulus);

	// 타이머 콜백 함수들
	void StartChasing(); // 포효/피격 끝난 후 추적 시작
	void FinishAttack(); // 공격 끝난 후 복귀

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|State")
	EMidBossAIState CurrentAIState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|State")
	TObjectPtr<AActor> TargetActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Config")
	float AttackRange = 150.0f;

	FTimerHandle StateTimerHandle;
};