// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SessionInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class USessionInterface : public UInterface
{
	GENERATED_BODY()
};

class PROJECT_BANG_SQUAD_API ISessionInterface
{
	GENERATED_BODY()

public:
	virtual void Host(FString ServerName, int32 MaxPlayers, FString HostName) = 0;
	virtual void Join(uint32 Index) = 0;
	virtual void RefreshServerList() = 0;
	virtual void OpenMainMenuLevel() = 0;
};
