#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Stage3AIController.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API AStage3AIController : public AAIController
{
	GENERATED_BODY()

public:
	AStage3AIController();

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaTime) override;

private:
	// 거리 계산 후 어떤 공격을 할지 결정하는 두뇌 함수
	void DecidePattern();

	// 애니메이션이 끝나고 다시 행동을 시작할 수 있게 깨워주는 함수
	void ResetAttackState();

	// 맵에서 가장 가까운 살아있는 플레이어를 찾는 함수
	AActor* FindNearestPlayer();


	void StartMovementPhase();
private:
	UPROPERTY()
	class AStage3MidBoss* ControlledBoss;

	UPROPERTY()
	AActor* CurrentTarget;

	// 현재 공격(몽타주)이 진행 중인지 체크하는 플래그
	bool bIsAttacking;

	// 몽타주 길이만큼 대기하기 위한 타이머
	FTimerHandle AttackWaitTimer;

	// 이 거리보다 멀면 원거리 공격, 가까우면 근접 공격을 합니다.
	float RangedAttackDistance = 400.0f;

	// [신규 추가] 현재 이동 중인지 체크
	bool bIsMoving;

	// [신규 추가] 2초 이동을 잴 타이머
	FTimerHandle MovementTimer;
};