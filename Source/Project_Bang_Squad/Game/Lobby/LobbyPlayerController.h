// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LobbyGameState.h"
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
	virtual void BeginPlay() override;

	//체험 직업 변경 요청
	UFUNCTION(BlueprintCallable)
	void RequestChangePreviewJob(EJobType NewJob);

	//준비
	UFUNCTION(BlueprintCallable)
	void RequestToggleReady();

	//직업 최종 확정
	UFUNCTION(BlueprintCallable)
	void RequestConfirmedJob(EJobType FinalJob);

	//닉네임 변경 요청
	UFUNCTION(Server, Reliable)
	void ServerSetNickname(const FString& NewName);

	//UI 갱신 요청
	void RefreshLobbyUI();
protected:
	
	UFUNCTION(Server, Reliable)
	void ServerPreviewJob(EJobType NewJob);

	UFUNCTION(Server, Reliable)
	void ServerToggleReady();

	UFUNCTION(Server, Reliable)
	void ServerConfirmedJob(EJobType FinalJob);

private:
	UPROPERTY(EditDefaultsOnly, Category = "BS|UI")
	TSubclassOf<class UUserWidget> LobbyMainWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category = "BS|UI")
	TSubclassOf<class UUserWidget> JobSelectWidgetClass;
	class ULobbyMainWidget* LobbyMainWidget;
	class UJobSelectWidget* JobSelectWidget;

	//GameState 초기화 대기용
	void InitLobbyUI();
	FTimerHandle InitTimerHandle;

	UFUNCTION()
	void OnLobbyPhaseChanged(ELobbyPhase NewPhase);

protected:
	virtual void SetupInputComponent() override;

private:
	bool bIsMenuVisible = true;

	//확인해보려고 BlueprintCallable추가해놓음
	UFUNCTION(BlueprintCallable)
	void ToggleLobbyMenu();
	void SetMenuState(bool bShow);
};
