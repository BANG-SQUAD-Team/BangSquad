#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Project_Bang_Squad/Shop/BangSquadShopData.h"
#include "ShopSlotWidget.generated.h"

// ★ [수정] ID(FName)와 데이터(FShopItemData)를 둘 다 보내도록 변경
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSlotSelected, FName, ItemID, const FShopItemData&, ItemData);

UCLASS()
class PROJECT_BANG_SQUAD_API UShopSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FShopItemData SlotItemData;

	FName SlotItemID;

	bool bIsOwnedItem = false;

	UPROPERTY(BlueprintAssignable, Category = "Event")
	FOnSlotSelected OnSlotSelected;

	// ★ [수정] Init 함수에 ID(RowName) 추가
	UFUNCTION(BlueprintCallable)
	void InitSlotData(FName NewID, const FShopItemData& NewData, bool bOwned, int32 Price);

protected:
	UPROPERTY(meta = (BindWidget))
	class UButton* Btn_Click = nullptr;

	UPROPERTY(meta = (BindWidget))
	class UImage* Img_Icon = nullptr;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* Txt_Name = nullptr;

	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnItemClicked();

	UPROPERTY(meta = (BindWidget))
	class UBorder* SelectionBorder;

	UPROPERTY(meta = (BindWidget))
	class UImage* Img_CheckMark;

public:
	void SetHighlight(bool bIsSelected);
	void SetOwnedStatus(bool bOwned);
};