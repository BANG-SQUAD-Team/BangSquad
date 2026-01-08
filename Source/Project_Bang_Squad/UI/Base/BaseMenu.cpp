// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/UI/Base/BaseMenu.h"

void UBaseMenu::SetOwningInstance(ISessionInterface* InInstance)
{
	OwningInstance = InInstance;
}

void UBaseMenu::StartUp()
{
	AddToViewport(10);
	bIsFocusable = true;
	FInputModeUIOnly Inputmode;
	Inputmode.SetWidgetToFocus(TakeWidget());
	Inputmode.SetLockMouseToViewportBehavior(
		EMouseLockMode::DoNotLock);

	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return;

	PC->SetInputMode(Inputmode);
	PC->bShowMouseCursor = true;
}

void UBaseMenu::Shutdown()
{
	RemoveFromViewport();
	bIsFocusable = false;
	
	FInputModeGameOnly Inputmode;
	
	UWorld* World = GetWorld();
	if (!World) return;
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return;

	PC->SetInputMode(Inputmode);
	PC->bShowMouseCursor = false;
}
