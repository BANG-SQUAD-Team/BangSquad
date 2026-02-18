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
    // ★ 1. [기능 추가] 선택 취소 (Toggle) 로직
    // 머리를 눌렀는데, 이미 그 머리가 선택되어 있었다면? -> 취소!
    if (SelectedItem.ItemType == EItemType::HeadGear)
    {
        if (bIsHeadSelected && SelectedHeadID == ItemID)
        {
            // 선택 해제
            bIsHeadSelected = false;
            SelectedHeadID = NAME_None;

            // 마네킹 머리 벗기기 (빈 데이터나 nullptr로 처리)
            // UpdateMannequinPreview(FShopItemData()); // 빈 데이터로 초기화 필요
            UE_LOG(LogTemp, Log, TEXT("머리 장식 선택 취소"));

            UpdatePurchaseButtonState(); // 버튼 갱신
            return; // 종료
        }

        // 새로운 선택
        bIsHeadSelected = true;
        SelectedHeadID = ItemID;
        SelectedHeadData = SelectedItem;
        UpdateMannequinPreview(SelectedItem);
    }
    else if (SelectedItem.ItemType == EItemType::Skin)
    {
        if (bIsSkinSelected && SelectedSkinID == ItemID)
        {
            // 선택 해제
            bIsSkinSelected = false;
            SelectedSkinID = NAME_None;

            // 마네킹 스킨 벗기기 (기본 스킨으로 복구 필요)
            // UpdateSkinPreview(FShopItemData()); 
            UE_LOG(LogTemp, Log, TEXT("스킨 선택 취소"));

            UpdatePurchaseButtonState();
            return;
        }

        // 새로운 선택
        bIsSkinSelected = true;
        SelectedSkinID = ItemID;
        SelectedSkinData = SelectedItem;
        UpdateSkinPreview(SelectedItem);
    }

    // ★ 2. 버튼 상태 갱신 (구매 모드인지 장착 모드인지 계산)
    UpdatePurchaseButtonState();
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

    // 1. 실제로 지불해야 할 금액 계산 (검증)
    int32 RequiredGold = 0;
    if (bIsHeadSelected && !PS->HasItem(SelectedHeadID))
        RequiredGold += GetPriceByRarity(SelectedHeadData.Rarity);

    if (bIsSkinSelected && !PS->HasItem(SelectedSkinID))
        RequiredGold += GetPriceByRarity(SelectedSkinData.Rarity);

    // 2. 돈 확인
    if (PS->GetCoin() < RequiredGold)
    {
        Txt_Price->SetText(FText::FromString(TEXT("Not Enough Gold!")));
        Txt_Price->SetColorAndOpacity(FLinearColor::Red);
        return; // 돈 없으면 중단
    }

    // =========================================================
    // ★ [핵심] 머리 처리 (구매 & 장착)
    // =========================================================
    if (bIsHeadSelected)
    {
        // 안 가졌으면 -> 구매 요청 (구매 후 자동 장착은 PlayerState에서 처리하거나 여기서 연달아 호출)
        if (!PS->HasItem(SelectedHeadID))
        {
            PS->Server_TryPurchase(SelectedHeadID, GetPriceByRarity(SelectedHeadData.Rarity), EItemType::HeadGear);
        }
        else
        {
            // 이미 가졌으면 -> 바로 장착 요청
            PS->Server_EquipItem(SelectedHeadID, EItemType::HeadGear);
        }
    }

    // =========================================================
    // ★ [핵심] 스킨 처리 (구매 & 장착) - if문이 분리되어 있어 둘 다 실행됨!
    // =========================================================
    if (bIsSkinSelected)
    {
        if (!PS->HasItem(SelectedSkinID))
        {
            // 주의: 머리를 샀다고 가정하고 돈이 깎인 상태여야 하지만, 
            // Server_TryPurchase는 서버에서 순차 처리되므로 안전합니다.
            PS->Server_TryPurchase(SelectedSkinID, GetPriceByRarity(SelectedSkinData.Rarity), EItemType::Skin);
        }
        else
        {
            PS->Server_EquipItem(SelectedSkinID, EItemType::Skin);
        }
    }

    // 3. UI 갱신 (버튼 텍스트를 "Equip Now"로 바꾸기 위해)
    // 약간의 딜레이 후 갱신하거나, PurchaseResult 델리게이트에서 호출
    UpdatePurchaseButtonState();
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