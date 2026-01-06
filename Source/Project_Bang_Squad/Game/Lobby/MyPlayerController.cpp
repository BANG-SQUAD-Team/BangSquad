// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/Game/Lobby/MyPlayerController.h"

#include "Project_Bang_Squad/Core/BSGameInstance.h"
#include "Project_Bang_Squad/UI/Menu/MainMenu.h"

void AMyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController() && MainMenuWidgetClass)
	{
		// 캐스팅을 먼저 해서 UMainMenu로 생성
		UMainMenu* MainMenuWidget = CreateWidget<UMainMenu>(this, MainMenuWidgetClass);
		if (MainMenuWidget)
		{
			// StartUp()이 AddToViewport + 입력모드 설정을 모두 처리함
			MainMenuWidget->StartUp();

			UGameInstance* GI = GetGameInstance();
			UBSGameInstance* MyGI = Cast<UBSGameInstance>(GI);
			if (MyGI)
			{
				// ✨ [핵심] 인스턴스에 위젯 등록
				MyGI->SetMainMenuWidget(MainMenuWidget);
				UE_LOG(LogTemp, Warning, TEXT("✅ Menu UI -> GameInstance 연결 성공!"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("❌ MainMenu 위젯 생성 실패!"));
		}
	}
	else
	{
		if (!IsLocalPlayerController())
			UE_LOG(LogTemp, Warning, TEXT("⚠️ 로컬 플레이어 컨트롤러가 아닙니다."));
		if (!MainMenuWidgetClass)
			UE_LOG(LogTemp, Error, TEXT("❌ MainMenuWidgetClass가 설정되지 않았습니다!"));
	}
}
