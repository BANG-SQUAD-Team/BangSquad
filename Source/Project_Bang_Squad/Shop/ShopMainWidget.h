#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Project_Bang_Squad/Shop/BangSquadShopData.h"
#include "Engine/DataTable.h" // 필수 헤더
#include "ShopMainWidget.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API UShopMainWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

public:
	UPROPERTY(meta = (BindWidget))
	class UScrollBox* Scroll_ItemList; // (혹시 스크롤박스 제어 필요하면 둠)

	UPROPERTY(meta = (BindWidget))
	class UWrapBox* Grid_ItemBox;

	UPROPERTY(meta = (BindWidget))
	class UImage* Img_CharPreview;

	UPROPERTY(EditDefaultsOnly, Category = "Shop")
	TSubclassOf<class UShopSlotWidget> SlotWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Shop")
	TSubclassOf<AActor> ShopStudioClass;

	// ★ [추가] 여기에 데이터 테이블을 넣을 겁니다.
	UPROPERTY(EditDefaultsOnly, Category = "Shop")
	UDataTable* ShopDataTable;

	UPROPERTY()
	AActor* ShopStudioInstance;

	UFUNCTION()
	void UpdateMannequinPreview(const FShopItemData& SelectedItem);

	void InitShopList(); // 이름 변경 (Test -> Shop)
};