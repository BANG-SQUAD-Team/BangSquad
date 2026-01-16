// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/Game/Stage/StagePlayerState.h"

#include "Net/UnrealNetwork.h"

void AStagePlayerState::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AStagePlayerState, Job);
	DOREPLIFETIME(AStagePlayerState, RespawnEndTime);
}

void AStagePlayerState::SetRespawnEndTime(float NewTime)
{
	RespawnEndTime = NewTime;
}

void AStagePlayerState::SetJob(EJobType NewJob)
{
	Job = NewJob;
}
