#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Game/Stage/StageGameMode.h"
#include "StageBossGameMode.generated.h"

class AStage1Boss;

// [신규] UI에 전달할 플레이어별 통계 데이터
USTRUCT(BlueprintType)
struct FPlayerQTEStat
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly)
	int32 TapCount = 0;
};

UCLASS()
class PROJECT_BANG_SQUAD_API AStageBossGameMode : public AStageGameMode
{
	GENERATED_BODY()

public:
	AStageBossGameMode();
	virtual void BeginPlay() override;

	// QTE 시스템
	void TriggerSpearQTE(AStage1Boss* BossActor);
	void ProcessQTEInput(AController* PlayerController);

	// 리스폰 관련 (기존 유지)
	virtual void RequestRespawn(AController* Controller);
	UFUNCTION(BlueprintCallable, Category = "BS|BossRule")
	void OnPlayerDied(AController* DeadController);

	void OnBossKilled();

protected:
	void EndSpearQTE(bool bSuccess);
	void OnQTETimeout();
	void CheckPartyWipe();
	void EndStage(bool bIsVictory);

	// [신규] UI 호출 이벤트 (블루프린트에서 위젯 생성 구현)
	UFUNCTION(BlueprintImplementableEvent, Category = "BS|UI")
	void ShowGameClearUI(const TArray<FPlayerQTEStat>& Stats);

	UFUNCTION(BlueprintImplementableEvent, Category = "BS|UI")
	void ShowRetryUI(const TArray<FPlayerQTEStat>& Stats);

protected:
	UPROPERTY()
	TObjectPtr<AStage1Boss> CurrentBoss;

	// [신규] 플레이어별 연타 횟수 저장 (Key: 컨트롤러, Value: 횟수)
	TMap<AController*, int32> PlayerTapCounts;

	int32 AccumulatedInputCount = 0;
	FTimerHandle QTETimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "BS|BossRule")
	int32 GoalQTECount = 40;

	UPROPERTY(EditDefaultsOnly, Category = "BS|BossRule")
	float QTEDuration = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "BS|BossRule")
	int32 MaxTeamLives = 10;
};