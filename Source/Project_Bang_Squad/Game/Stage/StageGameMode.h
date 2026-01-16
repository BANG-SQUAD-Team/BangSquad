// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "StageGameMode.generated.h"

class URespawnWidget;
enum class EJobType : uint8;
/**
 * 
 */
UCLASS()
class PROJECT_BANG_SQUAD_API AStageGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	//TODO: LobbyGameMode에도 똑같은 게 있어서 바꿀 필요 있음
	UPROPERTY(EditDefaultsOnly, Category = "BS|Class")
	TMap<EJobType, TSubclassOf<ACharacter>> JobCharacterMap;
	
	//실제 소환
	void SpawnPlayerCharacter(AController* Controller, EJobType MyJob);

	void RequestRespawn(AController* Controller);

	void ExecuteRespawn(AController* Controller);

	//스테이지 이동 함수
	UFUNCTION(BlueprintCallable, Category = "BS|Stage")
	void ClearStageAndMove(FString NextMapName);

	UPROPERTY(EditDefaultsOnly, Category = "BS|UI")
	TSubclassOf<class UUserWidget> RespawnWidgetClass;

protected:
	void RespawnPlayerElapsed(AController* DeadController);

	//부활 위치 계산
	FTransform GetRespawnTransform(AController* Controller);

private:
	UPROPERTY()
	URespawnWidget* RespawnWidgetInstance;
};
