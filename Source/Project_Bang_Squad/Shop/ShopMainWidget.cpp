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

    // [추가] 구매 버튼 클릭 이벤트 연결
    if (Btn_Purchase)
    {
        Btn_Purchase->OnClicked.AddDynamic(this, &UShopMainWidget::OnClick_PurchaseButton);
    }

    // [추가] 처음 가격은 0원
    if (Txt_Price)
    {
        Txt_Price->SetText(FText::FromString(TEXT("0 G")));
    }
}
void UShopMainWidget::InitShop(FName MyJobTag)
{
    // ★ 1. 내 직업 태그 저장 (목록 만들 때 쓰려고)
    CurrentJobTag = MyJobTag;

    UE_LOG(LogTemp, Warning, TEXT("[ShopUI] InitShop 시작! 태그: %s"), *MyJobTag.ToString());

    // --- 마네킹 찾기 로직 (기존 유지) ---
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

            UChildActorComponent* ChildComp = Studio->FindComponentByClass<UChildActorComponent>();
            if (ChildComp && ChildComp->GetChildActor())
            {
                if (USkeletalMeshComponent* SkelMesh = ChildComp->GetChildActor()->FindComponentByClass<USkeletalMeshComponent>())
                {
                    SkelMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
                }
            }
        }
        else
        {
            if (CaptureComp) CaptureComp->bCaptureEveryFrame = false;
        }
    }

    // ★ 2. 마네킹 다 찾았으니 이제 목록 생성! (필터링 적용)
    InitShopList();
}

void UShopMainWidget::InitShopList()
{
    // 박스 초기화
    if (Grid_ItemBox) Grid_ItemBox->ClearChildren();
    if (Grid_SkinBox) Grid_SkinBox->ClearChildren();

    // =========================================================
    // 1. 장비 아이템 테이블 처리 (ItemDataTable) -> 기존 SlotWidgetClass 사용
    // =========================================================
    if (ItemDataTable && SlotWidgetClass) // ★ 안전장치: 클래스 있는지 확인
    {
        TArray<FName> RowNames = ItemDataTable->GetRowNames();
        for (const FName& RowName : RowNames)
        {
            static const FString ContextString(TEXT("ItemInit"));
            FShopItemData* Data = ItemDataTable->FindRow<FShopItemData>(RowName, ContextString);
            if (!Data) continue;

            // 직업 필터링
            FString EnumString = UEnum::GetDisplayValueAsText(Data->RequiredJob).ToString();
            bool bIsMyJob = (CurrentJobTag.ToString() == EnumString);
            bool bIsCommon = (Data->RequiredJob == ECharacterJob::Common);

            if (bIsMyJob || bIsCommon)
            {
                // ★ 여기는 기존 SlotWidgetClass 사용
                UShopSlotWidget* NewSlot = CreateWidget<UShopSlotWidget>(this, SlotWidgetClass);
                if (NewSlot)
                {
                    NewSlot->InitSlotData(*Data);
                    NewSlot->OnSlotSelected.AddDynamic(this, &UShopMainWidget::UpdateMannequinPreview);
                    Grid_ItemBox->AddChildToWrapBox(NewSlot);
                }
            }
        }
    }

    // =========================================================
    // 2. 스킨 테이블 처리 (SkinDataTable) -> ★ SkinSlotWidgetClass 사용!
    // =========================================================
    if (SkinDataTable && SkinSlotWidgetClass) // ★ 안전장치: 스킨용 위젯 클래스 확인
    {
        TArray<FName> RowNames = SkinDataTable->GetRowNames();
        for (const FName& RowName : RowNames)
        {
            static const FString ContextString(TEXT("SkinInit"));
            FShopItemData* Data = SkinDataTable->FindRow<FShopItemData>(RowName, ContextString);
            if (!Data) continue;

            // 직업 필터링
            FString EnumString = UEnum::GetDisplayValueAsText(Data->RequiredJob).ToString();
            bool bIsMyJob = (CurrentJobTag.ToString() == EnumString);
            bool bIsCommon = (Data->RequiredJob == ECharacterJob::Common);

            if (bIsMyJob || bIsCommon)
            {
                // ★ [수정] 여기서 SkinSlotWidgetClass로 생성합니다!
                // 부모가 UShopSlotWidget이므로 리턴 타입은 그대로 둬도 됩니다.
                UShopSlotWidget* NewSlot = CreateWidget<UShopSlotWidget>(this, SkinSlotWidgetClass);

                if (NewSlot)
                {
                    NewSlot->InitSlotData(*Data);
                    NewSlot->OnSlotSelected.AddDynamic(this, &UShopMainWidget::UpdateSkinPreview);
                    Grid_SkinBox->AddChildToWrapBox(NewSlot);
                }
            }
        }
    }
}

void UShopMainWidget::UpdateMannequinPreview(const FShopItemData& SelectedItem)
{
    // 1. 마네킹 입히기 (기존 로직 유지)
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

    // 2. [추가] 데이터 저장 및 가격 갱신
    SelectedHeadData = SelectedItem;
    bIsHeadSelected = true;

    UpdateTotalPrice(); // 가격 다시 계산!
}

void UShopMainWidget::UpdateSkinPreview(const FShopItemData& SelectedSkin)
{
    // 1. 마네킹 입히기 (기존 로직 유지)
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

    // 2. [추가] 데이터 저장 및 가격 갱신
    SelectedSkinData = SelectedSkin;
    bIsSkinSelected = true;

    UpdateTotalPrice(); // 가격 다시 계산!
}

void UShopMainWidget::UpdateTotalPrice()
{
    // 1. 머리 장식 가격 (선택 안 했으면 0원)
    int32 HeadPrice = 0;
    if (bIsHeadSelected)
    {
        HeadPrice = GetPriceByRarity(SelectedHeadData.Rarity);
    }

    // 2. 스킨 가격 (선택 안 했으면 0원)
    int32 SkinPrice = 0;
    if (bIsSkinSelected)
    {
        SkinPrice = GetPriceByRarity(SelectedSkinData.Rarity);
    }

    // 3. 합산
    int32 TotalPrice = HeadPrice + SkinPrice;

    // 4. 텍스트 띄우기 (예: "Total: 1300 G")
    if (Txt_Price)
    {
        // 천 단위 콤마 찍으려면: FText::AsNumber(TotalPrice)
        FString PriceStr = FString::Printf(TEXT("Total: %d G"), TotalPrice);
        Txt_Price->SetText(FText::FromString(PriceStr));
    }
}

// 3. [수정] 구매 버튼 클릭 함수
void UShopMainWidget::OnClick_PurchaseButton()
{
    if (!bIsHeadSelected && !bIsSkinSelected) return;

    ABSPlayerState* PS = GetOwningPlayerState<ABSPlayerState>();
    if (!PS) return;

    // ★ 여기도 등급 기반으로 가격 다시 계산
    int32 TotalPrice = (bIsHeadSelected ? GetPriceByRarity(SelectedHeadData.Rarity) : 0) +
        (bIsSkinSelected ? GetPriceByRarity(SelectedSkinData.Rarity) : 0);

    // 1. 돈 부족한지 확인 (클라이언트)
    if (PS->GetCoin() < TotalPrice)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UI] 돈 부족!"));
        return;
    }

    // 2. 델리게이트 연결
    if (!PS->OnPurchaseResult.IsAlreadyBound(this, &UShopMainWidget::HandlePurchaseResult))
    {
        PS->OnPurchaseResult.AddDynamic(this, &UShopMainWidget::HandlePurchaseResult);
    }

    // 3. 서버에 요청
    PS->Server_TryPurchase(TotalPrice);
}

void UShopMainWidget::HandlePurchaseResult(bool bSuccess)
{
    if (bSuccess)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UI] 구매 성공! 아이템 지급 완료"));

       //추후 판매 기능 추가 예정
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[UI] 구매 실패! (서버 거절: 돈 부족 등)"));
        // "돈이 부족합니다" 팝업 띄우기
    }
}

int32 UShopMainWidget::GetPriceByRarity(EItemRarity Rarity)
{
    switch (Rarity)
    {
    case EItemRarity::Common:       return 100;  // 커먼: 100원
    case EItemRarity::Rare:         return 300;  // 레어: 300원
    case EItemRarity::Epic:         return 500;  // 에픽: 500원
    case EItemRarity::Legendary:    return 1000; // 전설: 1000원
    default:                        return 0;
    }
}