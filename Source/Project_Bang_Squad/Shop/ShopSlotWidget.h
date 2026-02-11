#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Project_Bang_Squad/Shop/BangSquadShopData.h"
#include "ShopSlotWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlotSelected, const FShopItemData&, ItemData);

UCLASS()
class PROJECT_BANG_SQUAD_API UShopSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 변수만 선언하고 ExposeOnSpawn은 뺍니다.
	FShopItemData SlotItemData;

	UPROPERTY(BlueprintAssignable, Category = "Event")
	FOnSlotSelected OnSlotSelected;

	// ★ [핵심] 외부에서 데이터를 꽂아주는 안전한 함수
	UFUNCTION(BlueprintCallable)
	void InitSlotData(const FShopItemData& NewData);

protected:
	// nullptr로 초기화 (안전장치)
	UPROPERTY(meta = (BindWidget))
	class UButton* Btn_Click = nullptr;

	UPROPERTY(meta = (BindWidget))
	class UImage* Img_Icon = nullptr;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* Txt_Name = nullptr;

	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnItemClicked();
};