#include "ShopMainWidget.h"
#include "ShopSlotWidget.h"
#include "Components/WrapBox.h"
#include "Kismet/GameplayStatics.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h" 
#include "Components/ChildActorComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"

void UShopMainWidget::NativeConstruct()
{
    Super::NativeConstruct();
    // 여기서 InitShopList()를 호출하면 직업 태그를 모르니 목록이 텅 빕니다.
    // InitShop() 안에서 호출하도록 구조를 바꿨습니다.
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
    // 1. 장비 아이템 테이블 처리 (ItemDataTable)
    // =========================================================
    if (ItemDataTable)
    {
        TArray<FName> RowNames = ItemDataTable->GetRowNames();
        for (const FName& RowName : RowNames)
        {
            static const FString ContextString(TEXT("ItemInit"));
            FShopItemData* Data = ItemDataTable->FindRow<FShopItemData>(RowName, ContextString);
            if (!Data) continue;

            // 직업 필터링 (내 직업 or 공용)
            FString EnumString = UEnum::GetDisplayValueAsText(Data->RequiredJob).ToString();
            bool bIsMyJob = (CurrentJobTag.ToString() == EnumString);
            bool bIsCommon = (Data->RequiredJob == ECharacterJob::Common);

            if (bIsMyJob || bIsCommon)
            {
                // 장비 박스(Grid_ItemBox)에 넣기
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
    // 2. 스킨 테이블 처리 (SkinDataTable) - 따로 관리!
    // =========================================================
    if (SkinDataTable)
    {
        TArray<FName> RowNames = SkinDataTable->GetRowNames();
        for (const FName& RowName : RowNames)
        {
            static const FString ContextString(TEXT("SkinInit"));
            FShopItemData* Data = SkinDataTable->FindRow<FShopItemData>(RowName, ContextString);
            if (!Data) continue;

            // 직업 필터링 (스킨은 보통 공용이 없고 직업 전용이 많음)
            FString EnumString = UEnum::GetDisplayValueAsText(Data->RequiredJob).ToString();
            bool bIsMyJob = (CurrentJobTag.ToString() == EnumString);
            bool bIsCommon = (Data->RequiredJob == ECharacterJob::Common);

            if (bIsMyJob || bIsCommon)
            {
                // 스킨 박스(Grid_SkinBox)에 넣기
                UShopSlotWidget* NewSlot = CreateWidget<UShopSlotWidget>(this, SlotWidgetClass);
                if (NewSlot)
                {
                    NewSlot->InitSlotData(*Data);
                    // 스킨용 미리보기 함수 연결
                    NewSlot->OnSlotSelected.AddDynamic(this, &UShopMainWidget::UpdateSkinPreview);
                    Grid_SkinBox->AddChildToWrapBox(NewSlot);
                }
            }
        }
    }
}

void UShopMainWidget::UpdateMannequinPreview(const FShopItemData& SelectedItem)
{
    if (!ShopStudioInstance) return;

    UChildActorComponent* ChildComp = ShopStudioInstance->FindComponentByClass<UChildActorComponent>();
    if (ChildComp)
    {
        if (ABaseCharacter* TargetChar = Cast<ABaseCharacter>(ChildComp->GetChildActor()))
        {
            // 기존 아이템 장착 함수
            TargetChar->EquipShopItem(SelectedItem);
        }
    }
}

// ★ [추가] 스킨 변경 함수
void UShopMainWidget::UpdateSkinPreview(const FShopItemData& SelectedSkin)
{
    if (!ShopStudioInstance) return;

    UChildActorComponent* ChildComp = ShopStudioInstance->FindComponentByClass<UChildActorComponent>();
    if (ChildComp)
    {
        if (ABaseCharacter* TargetChar = Cast<ABaseCharacter>(ChildComp->GetChildActor()))
        {
            // 캐릭터의 스킨 변경 함수 호출 (BaseCharacter에 추가했죠?)
            TargetChar->EquipSkin(SelectedSkin.SkinMaterial);
        }
    }
}