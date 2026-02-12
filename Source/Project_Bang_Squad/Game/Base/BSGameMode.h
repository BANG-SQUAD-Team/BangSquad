// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Project_Bang_Squad/Core/BSGameTypes.h"
#include "Project_Bang_Squad/Game/Interface/RespawnInterface.h"
#include "BSGameMode.generated.h"

class UBSGameInstance;

UCLASS()
class PROJECT_BANG_SQUAD_API ABSGameMode : public AGameModeBase, public IRespawnInterface
{
	GENERATED_BODY()

public:
	ABSGameMode();

	virtual void RequestRespawn(AController* Controller) override;
	virtual void SpawnPlayerCharacter(AController* Controller, EJobType JobType);
    
	// =========================================================================
	// 코인 시스템 (Banker Logic)
	// =========================================================================
	// 접속 시 코인 로드
	virtual void PostLogin(APlayerController* NewPlayer) override;
    
	// 접속 해제 시 코인 저장
	virtual void Logout(AController* Exiting) override;
    
	// 스테이지 클리어 보상 (자식 게임 모드에서 호출)
	
	UFUNCTION(BlueprintCallable, Category = "BS|Coin")
	void GiveStageClearReward(int32 Amount = 100);
    
	// 미니게임 보상 (순위별)
	UFUNCTION(BlueprintCallable, Category = "BS|Coin")
	void GiveMiniGameReward(const TArray<APlayerController*>& RankedPlayers);
    
	// 강제 저장
	//
	void SaveAllPlayerCoins();
    
protected:
	virtual FTransform GetRespawnTransform(AController* Controller);
	virtual void ExecuteRespawn(AController* Controller);

	UBSGameInstance* GetBSGameInstance() const;

	UPROPERTY(EditAnywhere, Category = "BS|Spawn")
	float BaseRespawnTime = 3.f;
	virtual float GetRespawnDelay(AController* Controller) { return BaseRespawnTime; }
};