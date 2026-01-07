// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/Game/Lobby/LobbyGameMode.h"

#include "LobbyGameState.h"
#include "LobbyPlayerController.h"
#include "LobbyPlayerState.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"

ALobbyGameMode::ALobbyGameMode()
{
	PlayerStateClass = ALobbyPlayerState::StaticClass();
	PlayerControllerClass = ALobbyPlayerController::StaticClass();
	GameStateClass = ALobbyGameState::StaticClass();
}

void ALobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	//TODO: 초기화 작업들 (PlayerState 데이터 갱신 등등...)
}

void ALobbyGameMode::ChangePlayerCharacter(AController* Controller, EJobType NewJob)
{
	if (!Controller || !JobCharacterMap.Contains(NewJob)) return;
	TSubclassOf<ACharacter> TargetClass = JobCharacterMap[NewJob];

	APawn* OldPawn = Controller->GetPawn();
	FVector Loc = OldPawn ? OldPawn->GetActorLocation() : FVector::ZeroVector;
	FRotator Rot = OldPawn ? OldPawn->GetActorRotation() : FRotator::ZeroRotator;
	if (OldPawn) OldPawn->Destroy();

	if (ACharacter* NewChar = GetWorld()->SpawnActor<ACharacter>(TargetClass, Loc, Rot))
	{
		Controller->Possess(NewChar);
	}
}

void ALobbyGameMode::CheckAllReady()
{
	ALobbyGameState* GS = GetGameState<ALobbyGameState>();
	if (!GS) return;

	if (GS->PlayerArray.Num() == 0) return;

	bool bAllReady = true;
	for (APlayerState* PS : GS->PlayerArray)
	{
		ALobbyPlayerState* LobbyPS = Cast<ALobbyPlayerState>(PS);
		if (!LobbyPS || !LobbyPS->bIsReady)
		{
			bAllReady = false;
			break;
		}
	}

	//이동
	if (bAllReady)
	{
		GS->SetLobbyPhase(ELobbyPhase::SelectJob);
		UE_LOG(LogTemp, Warning, TEXT("직업 선택 창 On."));
	}
}

void ALobbyGameMode::CheckConfirmedJob()
{
	ALobbyGameState* GS = GetGameState<ALobbyGameState>();
	if (!GS) return;

	if (GS->PlayerArray.Num() == 0) return;

	bool bAllConfirmed = true;
	for (APlayerState* PS : GS->PlayerArray)
	{
		ALobbyPlayerState* LobbyPS = Cast<ALobbyPlayerState>(PS);
		if (!LobbyPS || !LobbyPS->bIsConfirmedJob)
		{
			bAllConfirmed = false;
			break;
		}
	}

	//모두 직업 확정 완료
	if (bAllConfirmed)
	{
		//TODO: 나중에 TestMap -> Stage맵 이름으로 변경
		GetWorld()->ServerTravel("/Game/Maps/TestMap?listen");
	}
}
