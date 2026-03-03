#include "ShopMainWidget.h"
#include "ShopSlotWidget.h"
#include "Components/WrapBox.h"
#include "Kismet/GameplayStatics.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h" 
#include "Components/ChildActorComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Button.h"   
#include "Components/TextBlock.h" 
#include "Project_Bang_Squad/Game/Base/BSPlayerState.h"

void UShopMainWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 1. 구매 버튼 연결
    if (Btn_Purchase)
    {
        Btn_Purchase->OnClicked.RemoveDynamic(this, &UShopMainWidget::OnClick_PurchaseButton);
        Btn_Purchase->OnClicked.AddDynamic(this, &UShopMainWidget::OnClick_PurchaseButton);
    }

    // 2. 판매 버튼 연결
    if (Btn_Sell)
    {
        Btn_Sell->OnClicked.RemoveDynamic(this, &UShopMainWidget::OnClick_SellButton);
        Btn_Sell->OnClicked.AddDynamic(this, &UShopMainWidget::OnClick_SellButton);

        // 처음엔 비활성화 (안 눌리게)
        Btn_Sell->SetIsEnabled(false);
    }

    if (Txt_Price)
    {
        Txt_Price->SetText(FText::FromString(TEXT("0 G")));
    }

    if (ABSPlayerState* PS = GetOwningPlayerState<ABSPlayerState>())
    {
        PS->OnPurchaseResult.RemoveDynamic(this, &UShopMainWidget::HandlePurchaseResult);
        PS->OnPurchaseResult.AddDynamic(this, &UShopMainWidget::HandlePurchaseResult);
    }
}

void UShopMainWidget::UpdatePurchaseButtonState()
{
    ABSPlayerState* PS = GetOwningPlayerState<ABSPlayerState>();
    if (!PS || !Txt_Price) return;

    int32 TotalCost = 0;
    bool bHasUnownedItem = false; // 안 가진 게 하나라도 있는가?

    // 1. 머리 가격 계산
    if (bIsHeadSelected)
    {
        // 내 거 아니면 가격 추가
        if (!PS->HasItem(SelectedHeadID))
        {
            TotalCost += GetPriceByRarity(SelectedHeadData.Rarity);
            bHasUnownedItem = true;
        }
    }

    // 2. 스킨 가격 계산
    if (bIsSkinSelected)
    {
        if (!PS->HasItem(SelectedSkinID))
        {
            TotalCost += GetPriceByRarity(SelectedSkinData.Rarity);
            bHasUnownedItem = true;
        }
    }

    // 3. 텍스트 및 버튼 상태 변경
    if (!bIsHeadSelected && !bIsSkinSelected)
    {
        // 아무것도 선택 안 함
        Txt_Price->SetText(FText::FromString(TEXT("Select Item")));
        Btn_Purchase->SetIsEnabled(false);
    }
    else if (bHasUnownedItem)
    {
        // 구매 필요 (Purchase Mode)
        FString PriceStr = FString::Printf(TEXT("Purchase : %d G"), TotalCost);
        Txt_Price->SetText(FText::FromString(PriceStr));
        Txt_Price->SetColorAndOpacity(FLinearColor::White); // 흰색
        Btn_Purchase->SetIsEnabled(true);
    }
    else
    {
        // 다 내 거임 (Equip Mode)
        Txt_Price->SetText(FText::FromString(TEXT("Equip Now")));
        Txt_Price->SetColorAndOpacity(FLinearColor::Green); // 초록색
        Btn_Purchase->SetIsEnabled(true);
    }
}

void UShopMainWidget::InitShop(FName MyJobTag)
{
    CurrentJobTag = MyJobTag;

    TArray<AActor*> AllStudios;
    if (ShopStudioClass)
    {
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ShopStudioClass, AllStudios);
    }

    ShopStudioInstance = nullptr;

    for (AActor* Studio : AllStudios)
    {
        if (!Studio) continue;
        USceneCaptureComponent2D* CaptureComp = Studio->FindComponentByClass<USceneCaptureComponent2D>();

        if (Studio->ActorHasTag(MyJobTag))
        {
            ShopStudioInstance = Studio;
            if (CaptureComp)
            {
                CaptureComp->bCaptureEveryFrame = true;
                CaptureComp->SetHiddenInGame(false);
            }
        }
        else
        {
            if (CaptureComp) CaptureComp->bCaptureEveryFrame = false;
        }
    }

    InitShopList();
}

void UShopMainWidget::InitShopList()
{
    if (Grid_ItemBox) Grid_ItemBox->ClearChildren();
    if (Grid_SkinBox) Grid_SkinBox->ClearChildren();

    ABSPlayerState* PS = GetOwningPlayerState<ABSPlayerState>();

    if (!PS)
    {
        UE_LOG(LogTemp, Error, TEXT("[ShopUI] InitShopList: PlayerState가 아직 준비되지 않았습니다! (nullptr)"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[ShopUI] InitShopList: PlayerState는 확인됨. 인벤토리 데이터가 넘어왔는지 확인 필요."));
    }

    // 1. 아이템 테이블 처리
    if (ItemDataTable && SlotWidgetClass)
    {
        TArray<FName> RowNames = ItemDataTable->GetRowNames();
        for (const FName& RowName : RowNames)
        {
            FShopItemData* Data = ItemDataTable->FindRow<FShopItemData>(RowName, TEXT("ItemInit"));
            if (!Data) continue;

            FString EnumString = UEnum::GetDisplayValueAsText(Data->RequiredJob).ToString();
            bool bIsMyJob = (CurrentJobTag.ToString() == EnumString);
            bool bIsCommon = (Data->RequiredJob == ECharacterJob::Common);

            if (bIsMyJob || bIsCommon)
            {
                UShopSlotWidget* NewSlot = CreateWidget<UShopSlotWidget>(this, SlotWidgetClass);
                if (NewSlot)
                {
                    // ★ [핵심] PlayerState에게 "이 ID(RowName) 가지고 있니?" 물어보기
                    bool bOwned = false;
                    if (PS) bOwned = PS->HasItem(RowName); // PlayerState에 HasItem 구현 필요!

                    int32 Price = GetPriceByRarity(Data->Rarity);

                    // ★ [핵심] RowName을 같이 넘겨줍니다.
                    NewSlot->InitSlotData(RowName, *Data, bOwned, Price);

                    NewSlot->OnSlotSelected.AddDynamic(this, &UShopMainWidget::OnSlotClicked);
                    Grid_ItemBox->AddChildToWrapBox(NewSlot);
                }
            }
        }
    }

    // 2. 스킨 테이블 처리 (동일 방식)
    if (SkinDataTable && SkinSlotWidgetClass)
    {
        TArray<FName> RowNames = SkinDataTable->GetRowNames();
        for (const FName& RowName : RowNames)
        {
            FShopItemData* Data = SkinDataTable->FindRow<FShopItemData>(RowName, TEXT("SkinInit"));
            if (!Data) continue;

            FString EnumString = UEnum::GetDisplayValueAsText(Data->RequiredJob).ToString();
            bool bIsMyJob = (CurrentJobTag.ToString() == EnumString);
            bool bIsCommon = (Data->RequiredJob == ECharacterJob::Common);

            if (bIsMyJob || bIsCommon)
            {
                UShopSlotWidget* NewSlot = CreateWidget<UShopSlotWidget>(this, SkinSlotWidgetClass);
                if (NewSlot)
                {
                    bool bOwned = false;
                    if (PS) bOwned = PS->HasItem(RowName);

                    int32 Price = GetPriceByRarity(Data->Rarity);

                    // RowName 전달
                    NewSlot->InitSlotData(RowName, *Data, bOwned, Price);
                    NewSlot->OnSlotSelected.AddDynamic(this, &UShopMainWidget::OnSlotClicked);
                    Grid_SkinBox->AddChildToWrapBox(NewSlot);
                }
            }
        }
    }
}

void UShopMainWidget::OnSlotClicked(FName ItemID, const FShopItemData& SelectedItem)
{
    ABSPlayerState* PS = GetOwningPlayerState<ABSPlayerState>();
    bool bTargetOwned = PS ? PS->HasItem(ItemID) : false; // 클릭한 아이템 보유 여부

    bool bIsCurrentlySelected = false; // 방금 클릭한 것이 '선택 활성화' 되었는지 체크

    if (SelectedItem.ItemType == EItemType::HeadGear)
    {
        if (bIsHeadSelected && SelectedHeadID == ItemID)
        {
            bIsHeadSelected = false;
            SelectedHeadID = NAME_None;
            UpdateMannequinPreview(FShopItemData()); 
        }
        else
        {
            bIsHeadSelected = true;
            SelectedHeadID = ItemID;
            SelectedHeadData = SelectedItem;
            UpdateMannequinPreview(SelectedItem);
            bIsCurrentlySelected = true;
        }
        RefreshBoxHighlights(Grid_ItemBox, SelectedHeadID);
    }
    else if (SelectedItem.ItemType == EItemType::Skin)
    {
        if (bIsSkinSelected && SelectedSkinID == ItemID)
        {
            bIsSkinSelected = false;
            SelectedSkinID = NAME_None;
            // UpdateSkinPreview(FShopItemData()); 
        }
        else
        {
            bIsSkinSelected = true;
            SelectedSkinID = ItemID;
            SelectedSkinData = SelectedItem;
            UpdateSkinPreview(SelectedItem);
            bIsCurrentlySelected = true;
        }
        RefreshBoxHighlights(Grid_SkinBox, SelectedSkinID);
    }

    if (bIsCurrentlySelected && bTargetOwned)
    {
        // 내 거를 클릭했을 때
        CurrentShopState = EShopState::Selling;
        CurrentSelectedRowName = ItemID;
        SelectedSellData = SelectedItem;

        Btn_Sell->SetIsEnabled(true); // 판매 버튼 활성화

        UpdatePurchaseButtonState();
        UpdateTotalPrice();
    }
    else
    {
        CurrentShopState = EShopState::Buying;
        Btn_Sell->SetIsEnabled(false);
        UpdatePurchaseButtonState();
    }
}

void UShopMainWidget::UpdateMannequinPreview(const FShopItemData& SelectedItem)
{
    if (ShopStudioInstance)
    {
        UChildActorComponent* ChildComp = ShopStudioInstance->FindComponentByClass<UChildActorComponent>();
        if (ChildComp)
        {
            if (ABaseCharacter* TargetChar = Cast<ABaseCharacter>(ChildComp->GetChildActor()))
            {
                TargetChar->EquipShopItem(SelectedItem);
            }
        }
    }
}

void UShopMainWidget::UpdateSkinPreview(const FShopItemData& SelectedSkin)
{
    if (ShopStudioInstance)
    {
        UChildActorComponent* ChildComp = ShopStudioInstance->FindComponentByClass<UChildActorComponent>();
        if (ChildComp)
        {
            if (ABaseCharacter* TargetChar = Cast<ABaseCharacter>(ChildComp->GetChildActor()))
            {
                TargetChar->EquipSkin(SelectedSkin.SkinMaterial);
            }
        }
    }
}

void UShopMainWidget::UpdateTotalPrice()
{
    if (!Txt_Price) return;

    if (CurrentShopState == EShopState::Selling)
    {
        int32 SellPrice = GetSellPrice(GetPriceByRarity(SelectedSellData.Rarity));
        Txt_Price->SetText(FText::FromString(FString::Printf(TEXT("+ %d G"), SellPrice)));
        Txt_Price->SetColorAndOpacity(FLinearColor::Green);
    }
    else
    {
        int32 TotalPrice = (bIsHeadSelected ? GetPriceByRarity(SelectedHeadData.Rarity) : 0) +
            (bIsSkinSelected ? GetPriceByRarity(SelectedSkinData.Rarity) : 0);
        Txt_Price->SetText(FText::FromString(FString::Printf(TEXT("Total: %d G"), TotalPrice)));
        Txt_Price->SetColorAndOpacity(FLinearColor::White);
    }
}

void UShopMainWidget::OnClick_PurchaseButton()
{
    ABSPlayerState* PS = GetOwningPlayerState<ABSPlayerState>();
    if (!PS) return;

    if (!bIsHeadSelected && !bIsSkinSelected) return;

    int32 RequiredGold = 0;
    bool bNeedHeadBuy = bIsHeadSelected && !PS->HasItem(SelectedHeadID);
    bool bNeedSkinBuy = bIsSkinSelected && !PS->HasItem(SelectedSkinID);

    // 1. 필요한 골드 계산
    if (bNeedHeadBuy) RequiredGold += GetPriceByRarity(SelectedHeadData.Rarity);
    if (bNeedSkinBuy) RequiredGold += GetPriceByRarity(SelectedSkinData.Rarity);

    // 2. 돈 확인
    if (PS->GetCoin() < RequiredGold)
    {
        Txt_Price->SetText(FText::FromString(TEXT("Not Enough Gold!")));
        Txt_Price->SetColorAndOpacity(FLinearColor::Red);
        return;
    }

    // 3. 이미 보유한 아이템은 바로 장착 처리
    if (bIsHeadSelected && !bNeedHeadBuy) PS->Server_EquipItem(SelectedHeadID, EItemType::HeadGear);
    if (bIsSkinSelected && !bNeedSkinBuy) PS->Server_EquipItem(SelectedSkinID, EItemType::Skin);

    // 4. 구매 처리 (★ 핵심: 연달아 요청하지 않고 대기열 사용)
    PendingSkinPurchaseID = NAME_None; // 대기열 초기화

    if (bNeedHeadBuy && bNeedSkinBuy)
    {
        // 둘 다 사야 하면 머리부터 요청하고, 스킨 ID는 대기열에 저장
        PendingSkinPurchaseID = SelectedSkinID;
        PS->Server_TryPurchase(SelectedHeadID, GetPriceByRarity(SelectedHeadData.Rarity), EItemType::HeadGear);
    }
    else if (bNeedHeadBuy)
    {
        PS->Server_TryPurchase(SelectedHeadID, GetPriceByRarity(SelectedHeadData.Rarity), EItemType::HeadGear);
    }
    else if (bNeedSkinBuy)
    {
        PS->Server_TryPurchase(SelectedSkinID, GetPriceByRarity(SelectedSkinData.Rarity), EItemType::Skin);
    }
    else
    {
        // 둘 다 이미 있어서 장착만 한 경우
        UpdatePurchaseButtonState();
    }
}

void UShopMainWidget::OnClick_SellButton()
{
    ABSPlayerState* PS = GetOwningPlayerState<ABSPlayerState>();
    if (!PS) return;

    int32 SellAmount = GetSellPrice(GetPriceByRarity(SelectedSellData.Rarity));

    // ★ 저장해둔 ID(CurrentSelectedRowName)로 판매 요청
    // PlayerState에 Server_TrySell 함수가 구현되어 있어야 합니다.
    PS->Server_TrySell(CurrentSelectedRowName, SellAmount);

    UE_LOG(LogTemp, Warning, TEXT("[UI] 판매 요청 보냄: ID %s (+%d G)"), *CurrentSelectedRowName.ToString(), SellAmount);
}

void UShopMainWidget::HandlePurchaseResult(bool bSuccess)
{
    if (bSuccess)
    {
        // ★ 아직 스킨 구매가 남아있다면 바로 이어서 서버에 요청
        if (PendingSkinPurchaseID != NAME_None)
        {
            ABSPlayerState* PS = GetOwningPlayerState<ABSPlayerState>();
            if (PS)
            {
                PS->Server_TryPurchase(PendingSkinPurchaseID, GetPriceByRarity(SelectedSkinData.Rarity), EItemType::Skin);
            }
            PendingSkinPurchaseID = NAME_None; // 대기열 비우기
            return; // 여기서 return 해야 UI가 초기화되지 않습니다!
        }

        // 모든 구매가 완전히 끝났을 때만 UI 상태 초기화
        FTimerHandle RefreshTimer;
        GetWorld()->GetTimerManager().SetTimer(RefreshTimer, this, &UShopMainWidget::InitShopList, 0.2f, false);

        bIsHeadSelected = false;
        bIsSkinSelected = false;
        CurrentShopState = EShopState::Buying;

        if (Btn_Purchase) Btn_Purchase->SetIsEnabled(true);
        if (Btn_Sell) Btn_Sell->SetIsEnabled(false);

        UpdateTotalPrice();
        UpdatePurchaseButtonState();
    }
}

int32 UShopMainWidget::GetPriceByRarity(EItemRarity Rarity)
{
    switch (Rarity)
    {
    case EItemRarity::Default:      return 0;
    case EItemRarity::Common:       return 20;
    case EItemRarity::Rare:         return 60;
    case EItemRarity::Epic:         return 100;
    case EItemRarity::Legendary:    return 200;
    default:                        return 0;
    }
}

int32 UShopMainWidget::GetSellPrice(int32 OriginalPrice)
{
    return (int32)(OriginalPrice * 0.8f);
}

void UShopMainWidget::RefreshBoxHighlights(UWrapBox* TargetBox, FName SelectedID)
{
    if (!TargetBox) return;

    for (UWidget* Child : TargetBox->GetAllChildren())
    {
        if (UShopSlotWidget* ShopSlot = Cast<UShopSlotWidget>(Child))
        {
            ShopSlot->SetHighlight(ShopSlot->SlotItemID == SelectedID && SelectedID != NAME_None);
        }
    }
}

void UShopMainWidget::HandleSellResult(bool bSuccess)
{
    if (bSuccess)
    {
       
        if (SelectedSellData.ItemType == EItemType::HeadGear)
        {
            UpdateMannequinPreview(FShopItemData());
        }
        else if (SelectedSellData.ItemType == EItemType::Skin)
        {
            UpdateSkinPreview(FShopItemData());
        }

        if (ABaseCharacter* MyChar = Cast<ABaseCharacter>(GetOwningPlayerPawn()))
        {
            FTimerHandle SyncTimer;
            GetWorld()->GetTimerManager().SetTimer(SyncTimer, MyChar, &ABaseCharacter::UpdateAppearanceFromPlayerState, 0.2f, false);
        }

        bIsHeadSelected = false;
        bIsSkinSelected = false;
        SelectedHeadID = NAME_None;
        SelectedSkinID = NAME_None;
        CurrentShopState = EShopState::Buying;

        if (Btn_Sell) Btn_Sell->SetIsEnabled(false);

        InitShopList();

        UpdatePurchaseButtonState();
    }
}