// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/Game/Lobby/LobbyPlayerState.h"

#include "LobbyPlayerController.h"
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
		UE_LOG(LogTemp, Log, TEXT("[Server] PlayerState: 직업 확정 상태 변경 -> %s (Job: %d)"), 
					NewIsConfirmedJob ? TEXT("TRUE") : TEXT("FALSE"), (uint8)CurrentJob);
		
		bIsConfirmedJob = NewIsConfirmedJob;
		OnRep_UpdateUI();
	}
}

void ALobbyPlayerState::OnRep_UpdateUI()
{
	UE_LOG(LogTemp, Log, TEXT("[Client] OnRep_UpdateUI 호출됨! 내 Job: %d, Ready: %s, Confirmed: %s"), 
		(uint8)CurrentJob, 
		bIsReady ? TEXT("TRUE") : TEXT("FALSE"), 
		bIsConfirmedJob ? TEXT("TRUE") : TEXT("FALSE")
	);
	
	OnLobbyDataChanged.Broadcast();

	UWorld* World = GetWorld();
	if (World)
	{
		APlayerController* PC = World->GetFirstPlayerController();
		if (ALobbyPlayerController* LobbyPC = Cast<ALobbyPlayerController>(PC))
		{
			LobbyPC->RefreshLobbyUI();
		}
	}
}
