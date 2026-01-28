#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Game/Stage/StageGameState.h"
#include "StageBossGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBossHealthChanged, float, CurrentHP, float, MaxHP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQTEStateChanged, bool, bIsActive);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQTECountUpdated, int32, Current, int32, Target);
// [신규] 목숨 변경 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTeamLivesChanged, int32, NewLives);

UCLASS()
class PROJECT_BANG_SQUAD_API AStageBossGameState : public AStageGameState
{
	GENERATED_BODY()

public:
	AStageBossGameState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- [1] 보스 체력 ---
	UPROPERTY(ReplicatedUsing = OnRep_BossHealth, BlueprintReadOnly, Category = "Boss|State")
	float BossCurrentHealth;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Boss|State")
	float BossMaxHealth;

	UPROPERTY(BlueprintAssignable, Category = "Boss|Events")
	FOnBossHealthChanged OnBossHealthChanged;

	// --- [2] QTE 기믹 ---
	UPROPERTY(ReplicatedUsing = OnRep_IsQTEActive, BlueprintReadOnly, Category = "Boss|QTE")
	bool bIsQTEActive;

	UPROPERTY(ReplicatedUsing = OnRep_QTECounts, BlueprintReadOnly, Category = "Boss|QTE")
	int32 CurrentQTECount;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Boss|QTE")
	int32 TargetQTECount;

	UPROPERTY(BlueprintAssignable, Category = "Boss|Events")
	FOnQTEStateChanged OnQTEStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Boss|Events")
	FOnQTECountUpdated OnQTECountUpdated;

	// --- [3] 팀 공유 목숨 (신규 추가) ---
	UPROPERTY(ReplicatedUsing = OnRep_TeamLives, BlueprintReadOnly, Category = "Boss|Rules")
	int32 TeamLives;

	UPROPERTY(BlueprintAssignable, Category = "Boss|Events")
	FOnTeamLivesChanged OnTeamLivesChanged;

	// --- [서버 함수] ---
	void UpdateBossHealth(float NewCurrent, float NewMax);
	void SetQTEStatus(bool bActive, int32 InTarget);
	void UpdateQTECount(int32 NewCount);
	void SetTeamLives(int32 NewLives); // GameMode가 호출

protected:
	UFUNCTION() void OnRep_BossHealth();
	UFUNCTION() void OnRep_IsQTEActive();
	UFUNCTION() void OnRep_QTECounts();
	UFUNCTION() void OnRep_TeamLives(); // UI 갱신용
};