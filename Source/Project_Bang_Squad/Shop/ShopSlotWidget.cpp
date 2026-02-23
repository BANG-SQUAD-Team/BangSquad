#include "ShopSlotWidget.h"
#include "Components/Button.h"  
#include "Components/Image.h"     
#include "Components/TextBlock.h" 
#include "Components/Border.h"
#include "ShopTooltipWidget.h"

void UShopSlotWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Btn_Click)
    {
        Btn_Click->OnClicked.RemoveDynamic(this, &UShopSlotWidget::OnItemClicked);
        Btn_Click->OnClicked.AddDynamic(this, &UShopSlotWidget::OnItemClicked);
    }
}

void UShopSlotWidget::InitSlotData(FName NewID, const FShopItemData& NewData, bool bOwned, int32 Price)
{
    SlotItemID = NewID;   
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
        Txt_Name->SetText(FText::FromString(NameStr));

        if (bIsOwnedItem)
        {
            Txt_Name->SetColorAndOpacity(FLinearColor::Green);
        }
        else
        {
            Txt_Name->SetColorAndOpacity(FLinearColor::White);
        }
    }

    if (TooltipClass)
    {
        UShopTooltipWidget* TooltipWidget = CreateWidget<UShopTooltipWidget>(this, TooltipClass);
        if (TooltipWidget)
        {
            TooltipWidget->SetupTooltip(Price, bOwned, SlotItemData.Description);
            SetToolTip(TooltipWidget); // 언리얼 기본 함수: 마우스 Hover 시 이 위젯을 띄움
        }
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