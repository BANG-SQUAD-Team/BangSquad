// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/Game/Lobby/LobbyPlayerState.h"
#include "Net/UnrealNetwork.h"

ALobbyPlayerState::ALobbyPlayerState()
{
	bReplicates = true; //복제 설정
}

void ALobbyPlayerState::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALobbyPlayerState, CurrentJob);
	DOREPLIFETIME(ALobbyPlayerState, bIsReady);
	DOREPLIFETIME(ALobbyPlayerState, bIsConfirmedJob);
}

void ALobbyPlayerState::SetJob(EJobType NewJob)
{
	if (HasAuthority())
	{
		CurrentJob = NewJob;
		OnRep_UpdateUI();
	}
}

void ALobbyPlayerState::SetIsReady(bool NewIsReady)
{
	if (HasAuthority())
	{
		bIsReady = NewIsReady;
		OnRep_UpdateUI();
	}
}

void ALobbyPlayerState::SetIsConfirmedJob(bool NewIsConfirmedJob)
{
	if (HasAuthority())
	{
		bIsConfirmedJob = NewIsConfirmedJob;
		OnRep_UpdateUI();
	}
}

void ALobbyPlayerState::OnRep_UpdateUI()
{
	OnLobbyDataChanged.Broadcast();
}
