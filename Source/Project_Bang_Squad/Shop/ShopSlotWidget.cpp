// ShopSlotWidget.cpp

#include "ShopSlotWidget.h"
#include "Components/Button.h"  
#include "Components/Image.h"     
#include "Components/TextBlock.h" 
#include "Kismet/GameplayStatics.h"      
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h" 

void UShopSlotWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Btn_Click)
    {
        Btn_Click->OnClicked.AddDynamic(this, &UShopSlotWidget::OnItemClicked);
    }

    if (!SlotItemData.ItemName.IsEmpty())
    {
        if (Txt_Name) Txt_Name->SetText(SlotItemData.ItemName);
        if (Img_Icon) Img_Icon->SetBrushFromTexture(SlotItemData.Icon);
    }
}

void UShopSlotWidget::OnItemClicked()
{
	APawn* PlayerPawn = GetOwningPlayerPawn();

	if (ABaseCharacter* MyChar = Cast<ABaseCharacter>(PlayerPawn))
	{
		MyChar->Server_EquipShopItem(SlotItemData);

	}
}