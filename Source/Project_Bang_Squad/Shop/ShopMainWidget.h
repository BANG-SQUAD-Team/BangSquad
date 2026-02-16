#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Project_Bang_Squad/Shop/BangSquadShopData.h"
#include "Engine/DataTable.h" 
#include "ShopMainWidget.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API UShopMainWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

public:
	// --- UI 바인딩 ---
	UPROPERTY(meta = (BindWidget))
	class UWrapBox* Grid_ItemBox; // 머리 장식용

	UPROPERTY(meta = (BindWidget))
	class UWrapBox* Grid_SkinBox; // ★ [추가] 스킨용 (UMG에 추가 필수!)

	UPROPERTY(meta = (BindWidget))
	class UImage* Img_CharPreview;

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

	// ★ [추가] 필터링을 위해 내 직업을 기억해두는 변수
	FName CurrentJobTag;

	// --- 함수 ---
	UFUNCTION(BlueprintCallable)
	void InitShop(FName MyJobTag); // 1. 시작

	void InitShopList(); // 2. 목록 생성 (필터링 포함)

	// 3. 미리보기 업데이트
	UFUNCTION()
	void UpdateMannequinPreview(const FShopItemData& SelectedItem);

	// ★ [추가] 스킨 변경용
	UFUNCTION()
	void UpdateSkinPreview(const FShopItemData& SelectedSkin);

	UPROPERTY(meta = (BindWidget))
	class UButton* Btn_Purchase;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* Txt_Price;

private:
	UPROPERTY()
	class ABaseCharacter* MyCharacter;

	FShopItemData SelectedHeadData;
	bool bIsHeadSelected = false;

	FShopItemData SelectedSkinData;
	bool bIsSkinSelected = false;

	// [추가] 총 가격 계산 및 UI 갱신 함수
	void UpdateTotalPrice();

	// [추가] 버튼 클릭 시 실행할 함수
	UFUNCTION()
	void OnClick_PurchaseButton();

	// [추가] 구매 결과 처리 (성공/실패)
	UFUNCTION()
	void HandlePurchaseResult(bool bSuccess);

	int32 GetPriceByRarity(EItemRarity Rarity);
};