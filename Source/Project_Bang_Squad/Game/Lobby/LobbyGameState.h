// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "LobbyGameState.generated.h"

UENUM(BlueprintType)
enum class ELobbyPhase : uint8
{
	PreviewJob,
	SelectJob
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLobbyPhaseChanged, ELobbyPhase, NewPhase);

UCLASS()
class PROJECT_BANG_SQUAD_API ALobbyGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ALobbyGameState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//현재 로비 단계
	UPROPERTY(ReplicatedUsing = OnRep_CurrentPhase)
	ELobbyPhase CurrentPhase = ELobbyPhase::PreviewJob;

	UPROPERTY(BlueprintAssignable)
	FOnLobbyPhaseChanged OnLobbyPhaseChanged;

	void SetLobbyPhase(ELobbyPhase NewPhase);

protected:
	UFUNCTION()
	void OnRep_CurrentPhase();
};
