// BSPlayerState.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Project_Bang_Squad/Core/BSGameTypes.h"
#include "BSPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRespawnTimeChanged, float, NewRespawnTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPurchaseResult, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCoinChangedDelegate, int32, NewCoinAmount);

UCLASS()
class PROJECT_BANG_SQUAD_API ABSPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	// --- 기본 직업/리스폰 관련 ---
	UFUNCTION(Server, Reliable)
	void Server_SetJob(EJobType NewJob);

	virtual void OnRep_PlayerName() override;
	virtual void BeginPlay() override; // 시작 시 돈 지급용

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

	virtual void CopyProperties(APlayerState* PlayerState) override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	virtual void OnRep_JobType();

	UPROPERTY(Replicated)
	float RespawnEndTime = 0.f;

public:
	// =========================================================================
	// 코인 시스템 (Coin System)
	// =========================================================================
	UFUNCTION(BlueprintCallable, Category = "BS|Coin")
	int32 GetCoin() const { return CoinAmount; }

	void AddCoin(int32 Amount);
	void SetCoin(int32 Amount);

	UPROPERTY(BlueprintAssignable, Category = "BS|Coin")
	FOnCoinChangedDelegate OnCoinChanged;

	// =========================================================================
	// 상점 및 인벤토리 시스템 (Inventory & Shop)
	// =========================================================================
	UPROPERTY(BlueprintAssignable, Category = "BS|Shop")
	FOnPurchaseResult OnPurchaseResult;

	// [1] 구매 요청
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "BS|Shop")
	void Server_TryPurchase(int32 TotalCost);

	// ★ [2] 판매 요청 (오류 해결을 위해 필수!)
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "BS|Shop")
	void Server_TrySell(FName ItemID, int32 SellAmount);

	// ★ [3] 아이템 보유 확인 (오류 해결을 위해 필수!)
	UFUNCTION(BlueprintCallable, Category = "BS|Inventory")
	bool HasItem(FName ItemID) const;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_CoinAmount, VisibleAnywhere, BlueprintReadOnly, Category = "BS|Coin")
	int32 CoinAmount = 0;

	UFUNCTION()
	void OnRep_CoinAmount();

	// ★ [4] 내가 가진 아이템 목록 (서버 <-> 클라 동기화)
	UPROPERTY(Replicated)
	TArray<FName> OwnedItemIds;

	void AddItem(FName ItemID);
	void RemoveItem(FName ItemID);
};