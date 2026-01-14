// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Project_Bang_Squad/UI/Base/BaseMenu.h"
#include "Components/VerticalBox.h"
#include "LobbyMainWidget.generated.h"

enum class EJobType : uint8;
class UButton;
class UTextBlock;

UCLASS()
class PROJECT_BANG_SQUAD_API ULobbyMainWidget : public UBaseMenu
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

public:
	UPROPERTY(meta = (BindWidget))
	UWidget* MenuControlArea;
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_SelectTitan;
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_SelectStriker;
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_SelectMage;
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_SelectPaladin;
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_Ready;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* Txt_ReadyState;

	UPROPERTY(meta = (BindWidget))
	UVerticalBox* PlayerListContainer;

	UPROPERTY(EditDefaultsOnly, Category = "BS|UI")
	TSubclassOf<class ULobbyPlayerRow> PlayerRowClass;

	void UpdatePlayerList();

	
	//직업 선택하는 것만 껐다켰다
	void SetMenuVisibility(bool bVisible);

private:
	UFUNCTION()
	void OnBtn_SelectTitanClicked();
	UFUNCTION()
	void OnBtn_SelectStrikerClicked();
	UFUNCTION()
	void OnBtn_SelectMageClicked();
	UFUNCTION()
	void OnBtn_SelectPaladinClicked();
	UFUNCTION()
	void OnBtn_ReadyClicked();

	void SelectJobClickedHelper(EJobType Job);

	class ALobbyPlayerController* GetLobbyPC();
};
