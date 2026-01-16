// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Project_Bang_Squad/Core/BSGameInstance.h"
#include "StagePlayerState.generated.h"

/**
 * 
 */
UCLASS()
class PROJECT_BANG_SQUAD_API AStagePlayerState : public APlayerState
{
	GENERATED_BODY()

protected:
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated)
	EJobType Job = EJobType::None;

	UPROPERTY(Replicated)
	float RespawnEndTime = 0.f;

public:
	void SetRespawnEndTime(float NewTime);
	void SetJob(EJobType NewJob);
};
