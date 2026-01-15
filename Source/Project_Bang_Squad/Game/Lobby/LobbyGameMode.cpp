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

bool ALobbyGameMode::IsJobTaken(EJobType NewJob, class ALobbyPlayerState* RequestingPS)
{
	ALobbyGameState* GS = GetGameState<ALobbyGameState>();
	if (!GS) return false;

	for (APlayerState* PS : GS->PlayerArray)
	{
		ALobbyPlayerState* LobbyPS = Cast<ALobbyPlayerState>(PS);
		if (LobbyPS && LobbyPS != RequestingPS)
		{
			if (LobbyPS->bIsConfirmedJob && LobbyPS->CurrentJob == NewJob)
			{
				return true;
			}
		}
	}
	return false;
}

void ALobbyGameMode::ChangePlayerCharacter(AController* Controller, EJobType NewJob)
{
	if (!Controller || !JobCharacterMap.Contains(NewJob))
	{
		UE_LOG(LogTemp, Error, TEXT("[GameMode] 캐릭터 교체 실패! (Controller Null 혹은 Map에 없는 Job)"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[GameMode] 캐릭터 교체 시도... JobIndex: %d"), (uint8)NewJob);

	TSubclassOf<ACharacter> TargetClass = JobCharacterMap[NewJob];

	APawn* OldPawn = Controller->GetPawn();
	FVector Loc = OldPawn ? OldPawn->GetActorLocation() : FVector::ZeroVector;
	FRotator Rot = OldPawn ? OldPawn->GetActorRotation() : FRotator::ZeroRotator;
	if (OldPawn) OldPawn->Destroy();

	if (ACharacter* NewChar = GetWorld()->SpawnActor<ACharacter>(TargetClass, Loc, Rot))
	{
		Controller->Possess(NewChar);
		UE_LOG(LogTemp, Log, TEXT("[GameMode] 캐릭터 교체 성공!"));
	}
}

void ALobbyGameMode::CheckAllReady()
{
	ALobbyGameState* GS = GetGameState<ALobbyGameState>();
	if (!GS) return;

	if (GS->PlayerArray.Num() == 0) return;

	bool bAllReady = true;
	int32 ReadyCount = 0;

	for (APlayerState* PS : GS->PlayerArray)
	{
		ALobbyPlayerState* LobbyPS = Cast<ALobbyPlayerState>(PS);
		if (LobbyPS)
		{
			if (LobbyPS->bIsReady) ReadyCount++;
			else bAllReady = false;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[GameMode] 준비 체크 중... (%d / %d 명 준비됨)"), ReadyCount, GS->PlayerArray.Num());

	//이동

	//TODO: 4명으로 바꿔야함
	if (bAllReady && GS->PlayerArray.Num() == 4)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] ✅ 4인 전원 준비 완료! 직업 선택 페이즈로 전환합니다."));
		GS->SetLobbyPhase(ELobbyPhase::SelectJob);
	}

	else if (bAllReady && GS->PlayerArray.Num() < 4)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] 모든 인원이 준비되었으나 4명이 모이지 않아 대기합니다. (현재 %d명)"), GS->PlayerArray.Num());
	}
}

void ALobbyGameMode::CheckConfirmedJob()
{
	ALobbyGameState* GS = GetGameState<ALobbyGameState>();
	if (!GS) return;

	if (GS->PlayerArray.Num() == 0) return;

	int32 ConfirmedCount = 0;
	int32 TotalPlayers = GS->PlayerArray.Num();

	for (APlayerState* PS : GS->PlayerArray)
	{
		ALobbyPlayerState* LobbyPS = Cast<ALobbyPlayerState>(PS);
		if (LobbyPS && LobbyPS->bIsConfirmedJob)
		{
			ConfirmedCount++;
		}
		else
		{
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[GameMode] 직업 확정 현황: (%d / %d) 명"), ConfirmedCount, TotalPlayers);

	//모두 직업 확정 완료
	if (ConfirmedCount == TotalPlayers)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] ✅ 전원 직업 확정 완료! 게임(TestMap)으로 이동합니다. 🚀"));

		GetWorld()->ServerTravel("/Game/TeamShare/Level/Stage1_Demo?listen");
	}
}
