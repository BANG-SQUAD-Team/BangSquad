#include "ShopTooltipWidget.h"
#include "Components/TextBlock.h"

void UShopTooltipWidget::SetupTooltip(int32 OriginalPrice, bool bIsOwned, const FText& Description)
{
    if (Txt_Desc)
    {
        Txt_Desc->SetText(Description);
    }

    if (Txt_Price)
    {
        if (bIsOwned)
        {
            // 판매 모드: 80% 가격 계산 후 초록색 + 표시
            int32 SellPrice = OriginalPrice * 0.8f;
            Txt_Price->SetText(FText::FromString(FString::Printf(TEXT("+ %d G"), SellPrice)));
            Txt_Price->SetColorAndOpacity(FSlateColor(FLinearColor::Green));
        }
        else
        {
            // 구매 모드: 100% 원가 후 빨간색 - 표시
            Txt_Price->SetText(FText::FromString(FString::Printf(TEXT("- %d G"), OriginalPrice)));
            Txt_Price->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
        }
    }
}