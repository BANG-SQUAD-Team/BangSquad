// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Project_Bang_Squad/Core/SessionInterface.h"
#include "BaseMenu.generated.h"

/**
 * 
 */
UCLASS()
class PROJECT_BANG_SQUAD_API UBaseMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwningInstance(ISessionInterface* InInstance);

	void StartUp();
	void Shutdown();

protected:
	ISessionInterface* OwningInstance;
};
