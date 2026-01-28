#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Game/Stage/StageGameMode.h"
#include "StageBossGameMode.generated.h"

class AStage1Boss;

UCLASS()
class PROJECT_BANG_SQUAD_API AStageBossGameMode : public AStageGameMode
{
	GENERATED_BODY()

public:
	AStageBossGameMode();
	virtual void BeginPlay() override;

	// --- [1] QTE 기믹 ---
	void TriggerSpearQTE(AStage1Boss* BossActor);
	void ProcessQTEInput(AController* PlayerController);

	// --- [2] 사망 및 부활 (규칙 오버라이드) ---
	// BaseCharacter에서 사망 시 이 함수를 호출해야 함 (혹은 부모의 RequestRespawn을 오버라이드)
	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void OnPlayerDied(AController* DeadController);

	// 보스 처치 시 (Stage1Boss에서 호출)
	void OnBossKilled();

protected:
	// QTE 내부 로직
	void EndSpearQTE(bool bSuccess);
	void OnQTETimeout();

	// 부활 내부 로직
	void AttemptRespawn(AController* ControllerToRespawn);
	void CheckPartyWipe();
	void EndStage(bool bIsVictory);
	void RestartStage();

protected:
	UPROPERTY()
	TObjectPtr<AStage1Boss> CurrentBoss;

	// QTE 설정
	int32 GoalQTECount = 40;
	float QTEDuration = 10.0f;
	int32 AccumulatedInputCount = 0;
	FTimerHandle QTETimerHandle;

	// 부활 설정
	UPROPERTY(EditDefaultsOnly, Category = "GameRule")
	int32 MaxTeamLives = 10;

	UPROPERTY(EditDefaultsOnly, Category = "GameRule")
	float RespawnDelay = 8.0f;
};