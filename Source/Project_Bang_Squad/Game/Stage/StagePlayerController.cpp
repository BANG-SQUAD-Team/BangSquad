// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/Game/Stage/StagePlayerController.h"

#include "StageGameMode.h"
#include "Project_Bang_Squad/Core/BSGameInstance.h"

void AStagePlayerController::BeginPlay()
{
	Super::BeginPlay();

	//Local인 경우에만
	if (IsLocalPlayerController())
	{
		FInputModeGameOnly GameInputMode;
		SetInputMode(GameInputMode);
		bShowMouseCursor = false;
		
		//GameInstance에서 내가 고른 직업 꺼내오기
		if (UBSGameInstance* GI = Cast<UBSGameInstance>(GetGameInstance()))
		{
			//서버에게 소환 요청
			ServerRequestSpawn(GI->GetMyJob());
		}
	}
}

void AStagePlayerController::ServerRequestSpawn_Implementation(EJobType MyJob)
{
	//서버에서 게임모드를 찾아 실제 소환명령
	if (AStageGameMode* GM = GetWorld()->GetAuthGameMode<AStageGameMode>())
	{
		GM->SpawnPlayerCharacter(this, MyJob);
	}
}


