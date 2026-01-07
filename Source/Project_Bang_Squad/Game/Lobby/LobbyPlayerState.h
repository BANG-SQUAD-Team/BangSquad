// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Project_Bang_Squad/Core/BSGameInstance.h"
#include "LobbyPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLobbyDataChanged);

UCLASS()
class PROJECT_BANG_SQUAD_API ALobbyPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ALobbyPlayerState();
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	//현재 선택한 직업
	UPROPERTY(ReplicatedUsing = OnRep_UpdateUI, BlueprintReadOnly)
	EJobType CurrentJob = EJobType::None;

	//준비 완료 여부
	UPROPERTY(ReplicatedUsing = OnRep_UpdateUI, BlueprintReadOnly)
	bool bIsReady = false;

	//픽 확정 여부
	UPROPERTY(ReplicatedUsing = OnRep_UpdateUI, BlueprintReadOnly)
	bool bIsConfirmedJob = false;
	
	UPROPERTY(BlueprintAssignable)
	FOnLobbyDataChanged OnLobbyDataChanged;

	void SetJob(EJobType NewJob);
	void SetIsReady(bool NewIsReady);
	void SetIsConfirmedJob(bool NewIsConfirmedJob);

protected:
	UFUNCTION()
	void OnRep_UpdateUI();
};
