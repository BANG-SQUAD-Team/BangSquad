// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Project_Bang_Squad/Core/BSGameInstance.h"
#include "LobbyPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class PROJECT_BANG_SQUAD_API ALobbyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	//체험 직업 변경 요청
	UFUNCTION(BlueprintCallable)
	void RequestChangePreviewJob(EJobType NewJob);
	
	//준비
	UFUNCTION(BlueprintCallable)
	void RequestToggleReady();

	//직업 최종 확정
	UFUNCTION(BlueprintCallable)
	void RequestConfirmedJob(EJobType FinalJob);
protected:
	UFUNCTION(Server, Reliable)
	void ServerPreviewJob(EJobType NewJob);

	UFUNCTION(Server, Reliable)
	void ServerToggleReady();

	UFUNCTION(Server, Reliable)
	void ServerConfirmedJob(EJobType FinalJob);
};
