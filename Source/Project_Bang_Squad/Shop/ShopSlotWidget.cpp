#include "ShopSlotWidget.h"
#include "Components/Button.h"  
#include "Components/Image.h"     
#include "Components/TextBlock.h" 

void UShopSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 여기서는 버튼 연결만 합니다! (데이터 건드리면 터짐)
	if (Btn_Click)
	{
		Btn_Click->OnClicked.RemoveDynamic(this, &UShopSlotWidget::OnItemClicked);
		Btn_Click->OnClicked.AddDynamic(this, &UShopSlotWidget::OnItemClicked);
	}
}

// ★ 이 함수가 호출될 때 화면을 갱신합니다.
void UShopSlotWidget::InitSlotData(const FShopItemData& NewData)
{
	SlotItemData = NewData;

	if (Txt_Name)
	{
		Txt_Name->SetText(SlotItemData.ItemName);
	}

	if (Img_Icon)
	{
		if (SlotItemData.Icon) Img_Icon->SetBrushFromTexture(SlotItemData.Icon);
	}
}

void UShopSlotWidget::OnItemClicked()
{
	if (OnSlotSelected.IsBound())
	{
		OnSlotSelected.Broadcast(SlotItemData);
	}
}