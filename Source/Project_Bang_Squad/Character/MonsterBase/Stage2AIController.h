// Source/Project_Bang_Squad/Character/MonsterBase/Stage2AIController.h

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Stage2AIController.generated.h"

class AStage2MidBoss;

UENUM()
enum class EBossPatternState : uint8
{
	RangedBarrage, // 원거리 3회
	Teleporting,   // 접근
	MeleeSmash,    // 근접 공격
	Waiting        // 대기
};

UCLASS()
class PROJECT_BANG_SQUAD_API AStage2AIController : public AAIController
{
	GENERATED_BODY()

public:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaTime) override;

private:
	void RunPatternLogic();
	void SetNextPatternStep();
	void FindNewTarget();

	// 타이머 델리게이트 함수
	UFUNCTION()
	void OnActionFinished();

private:
	UPROPERTY()
	TObjectPtr<AStage2MidBoss> MyPawn;

	UPROPERTY()
	AActor* CurrentTarget;

	EBossPatternState CurrentState;
	int32 RangedAttackCount = 0; // 발사 횟수 카운트

	FTimerHandle ActionTimer;
	bool bIsBusy = false; // 행동 중인지 체크
};