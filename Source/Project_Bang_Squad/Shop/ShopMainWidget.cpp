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
}

void UShopMainWidget::InitShop(FName MyJobTag)
{
    CurrentJobTag = MyJobTag;

    // --- 마네킹 찾기 로직 (기존과 동일) ---
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

// ★ [수정] ID(ItemID)와 데이터(SelectedItem)를 모두 받습니다.
void UShopMainWidget::OnSlotClicked(FName ItemID, const FShopItemData& SelectedItem)
{
    ABSPlayerState* PS = GetOwningPlayerState<ABSPlayerState>();

    // ★ 1. ID로 정확하게 보유 여부 확인
    bool bIsOwned = false;
    if (PS) bIsOwned = PS->HasItem(ItemID);

    // =========================================================
    // CASE A: 이미 가진 아이템 -> [판매 모드] -> 판매 버튼 켜기
    // =========================================================
    if (bIsOwned)
    {
        CurrentShopState = EShopState::Selling;
        SelectedSellData = SelectedItem;
        CurrentSelectedRowName = ItemID; // 판매할 ID 저장

        // 구매 선택 초기화
        bIsHeadSelected = false;
        bIsSkinSelected = false;

        // ★ 버튼 상태 변경
        if (Btn_Purchase) Btn_Purchase->SetIsEnabled(false); // 구매 버튼 끄기(회색)
        if (Btn_Sell)     Btn_Sell->SetIsEnabled(true);      // 판매 버튼 켜기

        UE_LOG(LogTemp, Log, TEXT("[UI] 판매 모드: %s (ID: %s)"), *SelectedItem.ItemName.ToString(), *ItemID.ToString());
    }
    // =========================================================
    // CASE B: 없는 아이템 -> [구매 모드] -> 구매 버튼 켜기
    // =========================================================
    else
    {
        CurrentShopState = EShopState::Buying;

        if (SelectedItem.ItemType == EItemType::HeadGear)
        {
            SelectedHeadData = SelectedItem;
            bIsHeadSelected = true;
            UpdateMannequinPreview(SelectedItem);
        }
        else if (SelectedItem.ItemType == EItemType::Skin)
        {
            SelectedSkinData = SelectedItem;
            bIsSkinSelected = true;
            UpdateSkinPreview(SelectedItem);
        }

        // ★ 버튼 상태 변경
        if (Btn_Purchase) Btn_Purchase->SetIsEnabled(true);  // 구매 버튼 켜기
        if (Btn_Sell)     Btn_Sell->SetIsEnabled(false);     // 판매 버튼 끄기(회색)
    }

    UpdateTotalPrice();
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

    // 구매 로직만 남김
    if (!bIsHeadSelected && !bIsSkinSelected) return;

    int32 TotalPrice = (bIsHeadSelected ? GetPriceByRarity(SelectedHeadData.Rarity) : 0) +
        (bIsSkinSelected ? GetPriceByRarity(SelectedSkinData.Rarity) : 0);

    if (PS->GetCoin() < TotalPrice) return;

    if (!PS->OnPurchaseResult.IsAlreadyBound(this, &UShopMainWidget::HandlePurchaseResult))
    {
        PS->OnPurchaseResult.AddDynamic(this, &UShopMainWidget::HandlePurchaseResult);
    }

    PS->Server_TryPurchase(TotalPrice);
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
        InitShopList(); // 목록 갱신
        bIsHeadSelected = false;
        bIsSkinSelected = false;
        CurrentShopState = EShopState::Buying;

        // 초기화 후 버튼 상태도 다시 설정
        if (Btn_Purchase) Btn_Purchase->SetIsEnabled(true);
        if (Btn_Sell) Btn_Sell->SetIsEnabled(false);

        UpdateTotalPrice();
    }
}

int32 UShopMainWidget::GetPriceByRarity(EItemRarity Rarity)
{
    switch (Rarity)
    {
    case EItemRarity::Common:       return 100;
    case EItemRarity::Rare:         return 300;
    case EItemRarity::Epic:         return 500;
    case EItemRarity::Legendary:    return 1000;
    default:                        return 0;
    }
}

int32 UShopMainWidget::GetSellPrice(int32 OriginalPrice)
{
    return (int32)(OriginalPrice * 0.2f);
}