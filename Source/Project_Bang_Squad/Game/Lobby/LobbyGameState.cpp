// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/Game/Lobby/LobbyGameState.h"
#include "Net/UnrealNetwork.h"

ALobbyGameState::ALobbyGameState()
{
	bReplicates = true;
}

void ALobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALobbyGameState, CurrentPhase);
}

void ALobbyGameState::SetLobbyPhase(ELobbyPhase NewPhase)
{
	if (HasAuthority())
	{
		CurrentPhase = NewPhase;
		OnRep_CurrentPhase(); //서버는 RepNotify 자동호출X라서 수동으로 함
	}
}

void ALobbyGameState::OnRep_CurrentPhase()
{
	//모든 클라이언트(UI)에게 알림
	OnLobbyPhaseChanged.Broadcast(CurrentPhase);
}
