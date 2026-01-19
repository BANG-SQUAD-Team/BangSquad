// Source/Project_Bang_Squad/Game/StageBoss/StageBossGameMode.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "StageBossGameMode.generated.h"

/**
 * 보스 스테이지 전용 게임 모드
 * 역할: 팀 데스 카운트(공유 목숨), 부활 타이머, 전멸 체크, 승리 처리
 */
UCLASS()
class PROJECT_BANG_SQUAD_API AStageBossGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AStageBossGameMode();
	virtual void BeginPlay() override;

	// [Event] 보스 처치 시 호출 (승리) - Stage1Boss에서 호출
	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void OnBossKilled();

	// [Event] 플레이어 사망 시 호출 (부활 로직 시작) - BaseCharacter에서 호출
	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void OnPlayerDied(AController* DeadController);

protected:
	// --- [Game Rules] ---
	UPROPERTY(EditDefaultsOnly, Category = "GameRule")
	int32 MaxTeamLives = 10; // 초기 목숨 10개

	UPROPERTY(VisibleAnywhere, Category = "GameRule")
	int32 CurrentTeamLives;

	UPROPERTY(EditDefaultsOnly, Category = "GameRule")
	float RespawnDelay = 8.0f; // 부활 대기 시간

	// --- [Internal Logic] ---
	void AttemptRespawn(AController* ControllerToRespawn);
	void CheckPartyWipe(); // 전멸 체크
	void EndStage(bool bIsVictory);
	void RestartStage(); // 패배 시 재시작
};