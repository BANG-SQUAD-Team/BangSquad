// ShopSlotWidget.h

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Project_Bang_Squad/Shop/BangSquadShopData.h"
#include "ShopSlotWidget.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API UShopSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 데이터 변수 (이미 만드셨죠?)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop", meta = (ExposeOnSpawn = "true"))
	FShopItemData SlotItemData;

protected:
	UPROPERTY(meta = (BindWidget))
	class UButton* Btn_Click;

	UPROPERTY(meta = (BindWidget))
	class UImage* Img_Icon;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* Txt_Name;

	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnItemClicked();
};