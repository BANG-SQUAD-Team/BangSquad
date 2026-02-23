#include "ShopSlotWidget.h"
#include "Components/Button.h"  
#include "Components/Image.h"     
#include "Components/TextBlock.h" 
#include "Components/Border.h"

void UShopSlotWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Btn_Click)
    {
        Btn_Click->OnClicked.RemoveDynamic(this, &UShopSlotWidget::OnItemClicked);
        Btn_Click->OnClicked.AddDynamic(this, &UShopSlotWidget::OnItemClicked);
    }
}

// ★ [수정] NewID(RowName)를 받아서 저장합니다.
void UShopSlotWidget::InitSlotData(FName NewID, const FShopItemData& NewData, bool bOwned, int32 Price)
{
    SlotItemID = NewID;   // ★ ID 저장
    SlotItemData = NewData;
    bIsOwnedItem = bOwned;

    SetOwnedStatus(bIsOwnedItem);
    SetHighlight(false);

    // 1. 아이콘 설정
    if (Img_Icon && SlotItemData.Icon)
    {
        Img_Icon->SetBrushFromTexture(SlotItemData.Icon);

        // 보유 아이템이면 어둡게
        if (bIsOwnedItem)
        {
            Img_Icon->SetColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f));
        }
        else
        {
            Img_Icon->SetColorAndOpacity(FLinearColor::White);
        }
    }

    // 2. 텍스트 설정
    if (Txt_Name)
    {
        FString NameStr = SlotItemData.ItemName.ToString();
        FString FinalStr;

        if (bIsOwnedItem)
        {
            // 판매 가격 표시
            int32 SellPrice = Price * 0.2f;
            FinalStr = FString::Printf(TEXT("%s (+%d G)"), *NameStr, SellPrice);
            Txt_Name->SetColorAndOpacity(FLinearColor::Green);
        }
        else
        {
            // 구매 가격 표시
            FinalStr = FString::Printf(TEXT("%s"), *NameStr);
            Txt_Name->SetColorAndOpacity(FLinearColor::White);
        }

        Txt_Name->SetText(FText::FromString(FinalStr));
    }
}

void UShopSlotWidget::OnItemClicked()
{
    if (OnSlotSelected.IsBound())
    {
        // ★ [수정] 저장해둔 ID와 데이터를 같이 보냅니다!
        OnSlotSelected.Broadcast(SlotItemID, SlotItemData);
    }
}

void UShopSlotWidget::SetHighlight(bool bIsSelected)
{
    if (SelectionBorder)
    {
        SelectionBorder->SetVisibility(bIsSelected ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
    }
}

void UShopSlotWidget::SetOwnedStatus(bool bOwned)
{
    if (Img_CheckMark)
    {
        Img_CheckMark->SetVisibility(bOwned ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    }

    if (Img_Icon)
    {
        Img_Icon->SetColorAndOpacity(bOwned ? FLinearColor(0.5f, 0.5f, 0.5f, 1.0f) : FLinearColor::White);
    }
}