#include "ShopMainWidget.h"
#include "ShopSlotWidget.h"
#include "Components/WrapBox.h"
#include "Kismet/GameplayStatics.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h" 
#include "Components/ChildActorComponent.h"

void UShopMainWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ShopStudioClass)
	{
		ShopStudioInstance = UGameplayStatics::GetActorOfClass(GetWorld(), ShopStudioClass);
	}

	// 목록 생성 시작
	InitShopList();
}

void UShopMainWidget::InitShopList()
{
	// 안전장치: 그리드, 슬롯클래스, 데이터테이블 셋 중 하나라도 없으면 취소
	if (!Grid_ItemBox || !SlotWidgetClass || !ShopDataTable) return;

	Grid_ItemBox->ClearChildren();

	// 1. 데이터 테이블의 모든 행(Row) 이름을 가져옵니다.
	TArray<FName> RowNames = ShopDataTable->GetRowNames();

	// 2. 모든 행을 반복합니다.
	for (const FName& RowName : RowNames)
	{
		// 데이터 찾기
		static const FString ContextString(TEXT("ShopInit"));
		FShopItemData* ItemData = ShopDataTable->FindRow<FShopItemData>(RowName, ContextString);

		if (ItemData)
		{
			// 슬롯 생성
			UShopSlotWidget* NewSlot = CreateWidget<UShopSlotWidget>(this, SlotWidgetClass);
			if (NewSlot)
			{
				// ★ 데이터 주입 (포인터 값을 복사해서 넣음)
				NewSlot->InitSlotData(*ItemData);

				// 클릭 이벤트 연결
				NewSlot->OnSlotSelected.AddDynamic(this, &UShopMainWidget::UpdateMannequinPreview);

				// 화면 추가
				Grid_ItemBox->AddChildToWrapBox(NewSlot);
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
		AActor* Mannequin = ChildComp->GetChildActor();
		if (ABaseCharacter* TargetChar = Cast<ABaseCharacter>(Mannequin))
		{
			TargetChar->EquipShopItem(SelectedItem);
		}
	}
}