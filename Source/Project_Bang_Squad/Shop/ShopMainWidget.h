#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Project_Bang_Squad/Shop/BangSquadShopData.h"
#include "Engine/DataTable.h" 
#include "ShopMainWidget.generated.h"

// 상점 상태
UENUM(BlueprintType)
enum class EShopState : uint8
{
	Buying,
	Selling
};

UCLASS()
class PROJECT_BANG_SQUAD_API UShopMainWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

public:
	// --- UI 바인딩 ---
	UPROPERTY(meta = (BindWidget))
	class UWrapBox* Grid_ItemBox;

	UPROPERTY(meta = (BindWidget))
	class UWrapBox* Grid_SkinBox;

	UPROPERTY(meta = (BindWidget))
	class UImage* Img_CharPreview;

	// 구매 버튼
	UPROPERTY(meta = (BindWidget))
	class UButton* Btn_Purchase;

	// 판매 버튼
	UPROPERTY(meta = (BindWidget))
	class UButton* Btn_Sell;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* Txt_Price;

	// --- 설정 ---
	UPROPERTY(EditDefaultsOnly, Category = "Shop")
	TSubclassOf<class UShopSlotWidget> SlotWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Shop")
	TSubclassOf<class UShopSlotWidget> SkinSlotWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Shop")
	TSubclassOf<AActor> ShopStudioClass;

	UPROPERTY(EditDefaultsOnly, Category = "Shop")
	UDataTable* ItemDataTable;

	UPROPERTY(EditDefaultsOnly, Category = "Shop")
	UDataTable* SkinDataTable;

	// --- 내부 변수 ---
	UPROPERTY()
	AActor* ShopStudioInstance;

	FName CurrentJobTag;

	EShopState CurrentShopState = EShopState::Buying;

	// ★ 판매할 아이템 ID 저장
	FName CurrentSelectedRowName;

	// --- 함수 ---
	UFUNCTION(BlueprintCallable)
	void InitShop(FName MyJobTag);

	void InitShopList();

	// ★ [수정] 이제 ID도 같이 받습니다.
	UFUNCTION()
	void OnSlotClicked(FName ItemID, const FShopItemData& SelectedItem);

	void UpdateMannequinPreview(const FShopItemData& SelectedItem);
	void UpdateSkinPreview(const FShopItemData& SelectedSkin);
	void UpdateTotalPrice();

	UFUNCTION()
	void OnClick_PurchaseButton();

	UFUNCTION()
	void OnClick_SellButton();

	UFUNCTION()
	void HandlePurchaseResult(bool bSuccess);

	int32 GetPriceByRarity(EItemRarity Rarity);
	int32 GetSellPrice(int32 OriginalPrice);

private:
	FShopItemData SelectedHeadData;
	bool bIsHeadSelected = false;

	FShopItemData SelectedSkinData;
	bool bIsSkinSelected = false;

	FShopItemData SelectedSellData;
};