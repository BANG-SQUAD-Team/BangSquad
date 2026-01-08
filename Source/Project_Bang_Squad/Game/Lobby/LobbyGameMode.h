// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Project_Bang_Squad/Core/BSGameInstance.h"
#include "LobbyGameMode.generated.h"

/**
 * 
 */
UCLASS()
class PROJECT_BANG_SQUAD_API ALobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALobbyGameMode();

	//TODO: 바꿀 가능성O / 직업별 캐릭터 BP 설정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BS|Lobby")
	TMap<EJobType, TSubclassOf<ACharacter>> JobCharacterMap;

	//캐릭터 교체
	void ChangePlayerCharacter(AController* Controller, EJobType NewJob);

	//Ready 체크
	void CheckAllReady();

	//직업 확정했는지 체크
	void CheckConfirmedJob();

	virtual void PostLogin(APlayerController* NewPlayer) override;

};
