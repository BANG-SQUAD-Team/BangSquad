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
    if (!Grid_ItemBox || !SlotWidgetClass || !ShopDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("[ShopUI] 필수 변수 누락!"));
        return;
    }

    // 두 박스 모두 초기화
    Grid_ItemBox->ClearChildren();
    if (Grid_SkinBox) Grid_SkinBox->ClearChildren();

    TArray<FName> RowNames = ShopDataTable->GetRowNames();

    for (const FName& RowName : RowNames)
    {
        static const FString ContextString(TEXT("ShopInit"));
        FShopItemData* ItemData = ShopDataTable->FindRow<FShopItemData>(RowName, ContextString);

        if (!ItemData) continue;

        // =========================================================
        // ★ [핵심] 직업 필터링 (Enum -> String 변환 후 비교)
        // =========================================================

        // 데이터 테이블의 Enum 값을 문자열로 변환 (예: Mage -> "Mage")
        FString EnumString = UEnum::GetDisplayValueAsText(ItemData->RequiredJob).ToString();

        // 1. 내 직업 태그랑 같은가?
        bool bIsMyJob = (CurrentJobTag.ToString() == EnumString);

        // 2. 혹은 공용(Common) 인가?
        bool bIsCommon = (ItemData->RequiredJob == ECharacterJob::Common);

        // 내 것도 아니고 공용도 아니면 -> 건너뛰기
        if (!bIsMyJob && !bIsCommon) continue;

        // =========================================================
        // ★ [핵심] 아이템 타입별 분류 (장비 vs 스킨)
        // =========================================================
        UShopSlotWidget* NewSlot = CreateWidget<UShopSlotWidget>(this, SlotWidgetClass);
        if (NewSlot)
        {
            NewSlot->InitSlotData(*ItemData);

            if (ItemData->ItemType == EItemType::HeadGear)
            {
                // 머리 장식 -> 기존 박스, 기존 함수 연결
                NewSlot->OnSlotSelected.AddDynamic(this, &UShopMainWidget::UpdateMannequinPreview);
                Grid_ItemBox->AddChildToWrapBox(NewSlot);
            }
            else if (ItemData->ItemType == EItemType::Skin)
            {
                // 스킨 -> ★ 새 박스, 새 함수 연결
                NewSlot->OnSlotSelected.AddDynamic(this, &UShopMainWidget::UpdateSkinPreview);
                if (Grid_SkinBox) Grid_SkinBox->AddChildToWrapBox(NewSlot);
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