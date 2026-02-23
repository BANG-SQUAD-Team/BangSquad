#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShopTooltipWidget.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API UShopTooltipWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(meta = (BindWidget))
    class UTextBlock* Txt_Price;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* Txt_Desc;

    // 가격과 설명을 세팅하는 초기화 함수
    void SetupTooltip(int32 OriginalPrice, bool bIsOwned, const FText& Description);
};