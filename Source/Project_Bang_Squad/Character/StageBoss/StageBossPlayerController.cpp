// Source/Project_Bang_Squad/Game/StageBoss/StageBossPlayerController.cpp
#include "StageBossPlayerController.h"
#include "Project_Bang_Squad/Character/StageBoss/Stage1Boss.h" // 보스 헤더 참조
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"

void AStageBossPlayerController::SendBossQTEInput()
{
	// 현재 월드의 보스 액터를 찾아 입력 전달
	// (최적화를 위해 BeginPlay에서 보스를 캐싱해두는 방법도 있지만, 보스가 1명이므로 이 방식도 무방)
	AActor* BossActor = UGameplayStatics::GetActorOfClass(GetWorld(), AStage1Boss::StaticClass());
	if (AStage1Boss* Boss = Cast<AStage1Boss>(BossActor))
	{
		// 보스에게 "나 눌렀어!" 신호 보냄 (Server RPC)
		Boss->Server_SubmitQTEInput(this);
	}
}

void AStageBossPlayerController::Client_UpdateTeamLives_Implementation(int32 NewCount)
{
	// [TODO] 여기에 WBP_BossHUD 위젯을 가져와서 텍스트를 갱신하는 로직 추가 필요
	// 예: if (BossHUD) BossHUD->SetLifeCount(NewCount);

	// 일단 로그로 확인
	FString Msg = FString::Printf(TEXT("Team Lives Left: %d"), NewCount);
	GEngine->AddOnScreenDebugMessage(10, 5.f, FColor::Cyan, Msg);
}

void AStageBossPlayerController::Client_ShowRespawnTimer_Implementation(float Duration)
{
	// [TODO] 화면 중앙에 "부활까지 N초" 카운트다운 위젯 표시
	FString Msg = FString::Printf(TEXT("Respawning in %.1f seconds..."), Duration);
	GEngine->AddOnScreenDebugMessage(11, Duration, FColor::Yellow, Msg);
}

void AStageBossPlayerController::Client_OnStageEnded_Implementation(bool bIsVictory)
{
	// 1. 승리/패배 위젯 생성
	TSubclassOf<UUserWidget> WidgetToSpawn = bIsVictory ? VictoryWidgetClass : DefeatWidgetClass;

	if (WidgetToSpawn)
	{
		UUserWidget* EndWidget = CreateWidget<UUserWidget>(this, WidgetToSpawn);
		if (EndWidget)
		{
			EndWidget->AddToViewport(100); // 최상단 노출

			// 2. 입력 모드 전환 (UI만 조작 가능하도록)
			SetShowMouseCursor(true);
			FInputModeUIOnly InputMode;
			InputMode.SetWidgetToFocus(EndWidget->TakeWidget());
			SetInputMode(InputMode);
		}
	}


}