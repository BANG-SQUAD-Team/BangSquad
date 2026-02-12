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

    // ★ 원래 코드처럼 목록은 여기서 즉시 생성합니다! (슬롯 안 뜨는 문제 해결)
    InitShopList();
}

void UShopMainWidget::InitShop(FName MyJobTag)
{
    // [로그] 어떤 태그로 마네킹을 찾는지 확인
    UE_LOG(LogTemp, Warning, TEXT("[ShopUI] InitShop 시작! 받은 태그: %s"), *MyJobTag.ToString());

    TArray<AActor*> AllStudios;
    if (ShopStudioClass)
    {
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ShopStudioClass, AllStudios);
    }

    ShopStudioInstance = nullptr;

    // 태그에 맞는 스튜디오 찾기 및 카메라 켜기
    for (AActor* Studio : AllStudios)
    {
        if (!Studio) continue;

        UE_LOG(LogTemp, Warning, TEXT("[ShopUI] 후보 발견: %s, 첫 번째 태그: %s"),
            *Studio->GetName(),
            Studio->Tags.Num() > 0 ? *Studio->Tags[0].ToString() : TEXT("태그 없음"));

        USceneCaptureComponent2D* CaptureComp = Studio->FindComponentByClass<USceneCaptureComponent2D>();

        if (Studio->ActorHasTag(MyJobTag))
        {
            ShopStudioInstance = Studio;

            // 카메라 켜기
            if (CaptureComp)
            {
                CaptureComp->bCaptureEveryFrame = true;
                CaptureComp->SetHiddenInGame(false);
            }

            // 마네킹 애니메이션 켜기
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
            // 내 직업 아니면 카메라 끄기
            if (CaptureComp) CaptureComp->bCaptureEveryFrame = false;
        }
    }
}

void UShopMainWidget::InitShopList()
{
    // 안전장치
    if (!Grid_ItemBox || !SlotWidgetClass || !ShopDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("[ShopUI] 필수 변수가 비어있습니다 (WrapBox, SlotClass, DataTable)"));
        return;
    }

    Grid_ItemBox->ClearChildren();

    TArray<FName> RowNames = ShopDataTable->GetRowNames();
    UE_LOG(LogTemp, Warning, TEXT("[ShopUI] 슬롯 생성 시작: %d 개"), RowNames.Num());

    for (const FName& RowName : RowNames)
    {
        static const FString ContextString(TEXT("ShopInit"));
        FShopItemData* ItemData = ShopDataTable->FindRow<FShopItemData>(RowName, ContextString);

        if (ItemData)
        {
            UShopSlotWidget* NewSlot = CreateWidget<UShopSlotWidget>(this, SlotWidgetClass);
            if (NewSlot)
            {
                NewSlot->InitSlotData(*ItemData);
                NewSlot->OnSlotSelected.AddDynamic(this, &UShopMainWidget::UpdateMannequinPreview);
                Grid_ItemBox->AddChildToWrapBox(NewSlot);
            }
        }
    }
}

void UShopMainWidget::UpdateMannequinPreview(const FShopItemData& SelectedItem)
{
    if (!ShopStudioInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("[ShopUI] 마네킹 스튜디오가 연결되지 않았습니다!"));
        return;
    }

    UChildActorComponent* ChildComp = ShopStudioInstance->FindComponentByClass<UChildActorComponent>();
    if (ChildComp)
    {
        if (ABaseCharacter* TargetChar = Cast<ABaseCharacter>(ChildComp->GetChildActor()))
        {
            TargetChar->EquipShopItem(SelectedItem);
        }
    }
}