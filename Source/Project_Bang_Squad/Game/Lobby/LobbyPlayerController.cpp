// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/Game/Lobby/LobbyPlayerController.h"

#include "LobbyGameMode.h"
#include "LobbyPlayerState.h"

void ALobbyPlayerController::RequestChangePreviewJob(EJobType NewJob)
{
	ServerPreviewJob(NewJob);
}

void ALobbyPlayerController::RequestToggleReady()
{
	ServerToggleReady();
}

void ALobbyPlayerController::RequestConfirmedJob(EJobType FinalJob)
{
	if (UBSGameInstance* GI = Cast<UBSGameInstance>(GetGameInstance()))
	{
		GI->SetMyJob(FinalJob);
		ServerConfirmedJob(FinalJob);
	}}

void ALobbyPlayerController::ServerPreviewJob_Implementation(EJobType NewJob)
{
	if (ALobbyPlayerState* PS = GetPlayerState<ALobbyPlayerState>())
	{
		PS->SetJob(NewJob);
	}

	if (ALobbyGameMode* GM = GetWorld()->GetAuthGameMode<ALobbyGameMode>())
	{
		GM->ChangePlayerCharacter(this, NewJob);
	}
}

void ALobbyPlayerController::ServerToggleReady_Implementation()
{
	if (ALobbyPlayerState* PS = GetPlayerState<ALobbyPlayerState>())
	{
		PS->SetIsReady(!PS->bIsReady);

		if (ALobbyGameMode* GM = GetWorld()->GetAuthGameMode<ALobbyGameMode>())
		{
			GM->CheckAllReady();
		}
	}
}

void ALobbyPlayerController::ServerConfirmedJob_Implementation(EJobType FinalJob)
{
	if (ALobbyPlayerState* PS = GetPlayerState<ALobbyPlayerState>())
	{
		PS->SetJob(FinalJob);
		PS->SetIsConfirmedJob(true);

		if (ALobbyGameMode* GM = GetWorld()->GetAuthGameMode<ALobbyGameMode>())
		{
			GM->
		}
	}
}
