// Source/Project_Bang_Squad/Game/StageBoss/StageBossPlayerController.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "StageBossPlayerController.generated.h"

/**
 * 보스 스테이지 전용 플레이어 컨트롤러
 * 역할: QTE 입력 전송, 팀 목숨 UI 수신, 승리/패배 결과창 표시
 */
UCLASS()
class PROJECT_BANG_SQUAD_API AStageBossPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// [Input] QTE 스페이스바 입력 전송 (Blueprint에서 호출: IA_QTE -> Call This)
	UFUNCTION(BlueprintCallable, Category = "Boss|Input")
	void SendBossQTEInput();

	// [UI] 남은 팀 목숨 갱신 (Server -> Client)
	UFUNCTION(Client, Reliable)
	void Client_UpdateTeamLives(int32 NewCount);

	// [UI] 부활 타이머 표시 (Server -> Client)
	UFUNCTION(Client, Reliable)
	void Client_ShowRespawnTimer(float Duration);

	// [UI] 게임 종료(승리/패배) 알림 (Server -> Client)
	UFUNCTION(Client, Reliable)
	void Client_OnStageEnded(bool bIsVictory);

protected:
	// [Settings] 승리/패배 시 띄울 위젯 클래스 (BP에서 할당)
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> VictoryWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> DefeatWidgetClass;
};