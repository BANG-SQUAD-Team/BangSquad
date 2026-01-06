// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Project_Bang_Squad/Core/SessionInterface.h"
#include "Project_Bang_Squad/UI/Base/BaseMenu.h"
#include "MainMenu.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API UMainMenu : public UBaseMenu
{
	GENERATED_BODY()
public:
	UMainMenu();
protected:
	virtual bool Initialize() override;

	ISessionInterface* OwningInstance;

public:
	UFUNCTION()
	void SetServerList(TArray<FServerData> InServerData);

	UFUNCTION()
	void OpenMainMenu();

	UFUNCTION()
	void OpenHostMenu();

	UFUNCTION()
	void OpenJoinMenu();

	UFUNCTION()
	void HostServer();

	UFUNCTION()
	void QuitGame();

	UFUNCTION()
	void JoinServer();

	void SetSelectedIndex(uint32 InIndex);

	UFUNCTION()
	void OnGuestNameChanged(const FText& Text); // 닉네임 칠 때마다 호출

	void UpdateJoinButtonState(); 

private:
	void SetButtonColorState(class UButton* InButton, bool bIsSelected);
	
	TOptional<uint32> SelectedIndex;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UUserWidget> ServerRowClass;

	int32 MaxPlayers = 4;

#pragma region Menu Widgets
	UPROPERTY(meta = (BindWidget))
	class UWidgetSwitcher* MenuSwitcher;
	
	UPROPERTY(meta = (BindWidget))
	class UButton* HostButton;
	UPROPERTY(meta = (BindWidget))
	class UButton* JoinButton; //조인 메뉴오픈

	UPROPERTY(meta = (BindWidget))
	class UButton* ConfirmJoinButton; //해당 방에 조인하는거

	UPROPERTY(meta = (BindWidget))
	class UEditableTextBox* GuestNameInput; // 조인 메뉴의 닉네임 입력칸

	UPROPERTY(meta = (BindWidget))
	class UButton* QuitButton;

	UPROPERTY(meta = (BindWidget))
	class UButton* CancelJoinButton;
	UPROPERTY(meta = (BindWidget))
	class UButton* CancelHostButton;


	UPROPERTY(meta = (BindWidget))
	class UWidget* MainMenu;

	UPROPERTY(meta = (BindWidget))
	class UWidget* HostMenu;

	UPROPERTY(meta = (BindWidget))
	class UWidget* JoinMenu;

	//HostMenu
	UPROPERTY(meta = (BindWidget))
	class UEditableTextBox* ServerHostName;

	UPROPERTY(meta = (BindWidget))
	class UButton* ConfirmHostButton;

	UPROPERTY(meta = (BindWidget))
	class UEditableTextBox* OwnerNameInput;

	UPROPERTY(meta = (BindWidget))
	class UPanelWidget* ServerList;

#pragma endregion 
};
