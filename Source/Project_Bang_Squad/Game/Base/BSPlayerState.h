// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Project_Bang_Squad/Core/BSGameTypes.h"
#include "BSPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRespawnTimeChanged, float, NewRespawnTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPurchaseResult, bool, bSuccess);

// 코인 변경 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCoinChangedDelegate, int32, NewCoinAmount);
UCLASS()
class PROJECT_BANG_SQUAD_API ABSPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	UFUNCTION(Server, Reliable)
	void Server_SetJob(EJobType NewJob);
	
	virtual void OnRep_PlayerName() override;

	UFUNCTION()
	FORCEINLINE EJobType GetJob() { return JobType; }
	virtual void SetJob(EJobType NewJob);
	
	void SetRespawnEndTime(float NewTime);
	float GetRespawnEndTime() const { return RespawnEndTime; }

	UPROPERTY(BlueprintAssignable)
	FOnRespawnTimeChanged OnRespawnTimeChanged;
protected:
	UPROPERTY(ReplicatedUsing = OnRep_JobType)
	EJobType JobType = EJobType::None;
	
	//SeamlessTravel 시 데이터를 넘겨주는 함수
	virtual void CopyProperties(APlayerState* PlayerState) override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	
	UFUNCTION()
	virtual void OnRep_JobType();

	UPROPERTY(Replicated)
	float RespawnEndTime = 0.f;
	
	// =========================================================================
	// 코인 시스템 (Coin System)
	// =========================================================================
public:
	// 코인 Getter
	UFUNCTION(BlueprintCallable, Category = "BS|Coin")
	int32 GetCoin() const { return CoinAmount;}
	
	// 코인 추가 (서버 권한 필요)
	void AddCoin(int32 Amount);
	
	// 코인 강제 설정 (로드용, 서버 권한 필요)
	void SetCoin(int32 Amount);
	
	// UI 바인딩용 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "BS|Coin")
	FOnCoinChangedDelegate OnCoinChanged;

	// =========================================================================
	// 상점 구매 시스템 (Shop Purchase System)
	// =========================================================================

	// 1. UI가 구매 결과를 듣기 위한 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "BS|Shop")
	FOnPurchaseResult OnPurchaseResult;

	// 2. 서버에게 "이 가격만큼 결제해줘"라고 요청하는 함수
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "BS|Shop")
	void Server_TryPurchase(int32 TotalCost);
	
protected:
	UPROPERTY(ReplicatedUsing = OnRep_CoinAmount, VisibleAnywhere, BlueprintReadOnly, Category = "BS|Coin")
	int32 CoinAmount = 0;
	
	UFUNCTION()
	void OnRep_CoinAmount();
};
