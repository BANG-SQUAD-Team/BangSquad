#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Game/Stage/StageGameMode.h"
#include "StageBossGameMode.generated.h"

class AStage1Boss;

// [�ű�] UI�� ������ �÷��̾ ��� ������
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

	// QTE �ý���
	void TriggerSpearQTE(AStage1Boss* BossActor);
	void ProcessQTEInput(AController* PlayerController);

	// ������ ���� (���� ����)
	virtual void RequestRespawn(AController* Controller) override;
	UFUNCTION(BlueprintCallable, Category = "BS|BossRule")
	void OnPlayerDied(AController* DeadController);

	void OnBossKilled();

	UFUNCTION(Exec)
	void DebugStartQTE();
protected:
	void EndSpearQTE(bool bSuccess);
	void OnQTETimeout();
	void CheckPartyWipe();
	void EndStage(bool bIsVictory);

	// [�ű�] UI ȣ�� �̺�Ʈ (�������Ʈ���� ���� ���� ����)
	UFUNCTION(BlueprintImplementableEvent, Category = "BS|UI")
	void ShowGameClearUI(const TArray<FPlayerQTEStat>& Stats);

	UFUNCTION(BlueprintImplementableEvent, Category = "BS|UI")
	void ShowRetryUI(const TArray<FPlayerQTEStat>& Stats);

protected:
	UPROPERTY()
	TObjectPtr<AStage1Boss> CurrentBoss;

	// [�ű�] �÷��̾ ��Ÿ Ƚ�� ���� (Key: ��Ʈ�ѷ�, Value: Ƚ��)
	TMap<AController*, int32> PlayerTapCounts;

	int32 AccumulatedInputCount = 0;
	FTimerHandle QTETimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "BS|BossRule")
	int32 GoalQTECount = 100;

	UPROPERTY(EditDefaultsOnly, Category = "BS|BossRule")
	float QTEDuration = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "BS|BossRule")
	int32 MaxTeamLives = 10;

	//부활 시도 함수
	void AttemptRespawn(AController* ControllerToRespawn);

	void ReturnToStage();
};