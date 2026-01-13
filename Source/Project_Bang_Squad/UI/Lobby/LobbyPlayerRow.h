// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "JobSelectWidget.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "LobbyPlayerRow.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API ULobbyPlayerRow : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	UTextBlock* Txt_Name;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* Txt_ReadyState;

	//데이터 갱신 함수
	void UpdateInfo(FString PlayerName, bool bIsReady, EJobType JobType);
};
