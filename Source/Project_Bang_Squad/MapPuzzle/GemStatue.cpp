#include "Project_Bang_Squad/MapPuzzle/GemStatue.h"
#include "Project_Bang_Squad/MapPuzzle/CenterStatueManager.h"
#include "Components/StaticMeshComponent.h"

AGemStatue::AGemStatue()
{
	PrimaryActorTick.bCanEverTick = false;

	StatueMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatueMesh"));
	RootComponent = StatueMesh;

	HiddenAddonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HiddenAddonMesh"));
	HiddenAddonMesh->SetupAttachment(RootComponent);
	HiddenAddonMesh->SetHiddenInGame(true); // 처음엔 숨김
}

void AGemStatue::BeginPlay()
{
	Super::BeginPlay();

	CurrentGemCount = 0;

	// 등록된 보석들에게 "파괴되면 나한테 알려줘"라고 이벤트 걸기
	for (AActor* Gem : TargetGems)
	{
		if (IsValid(Gem))
		{
			Gem->OnDestroyed.AddDynamic(this, &AGemStatue::CheckGemStatus);
			CurrentGemCount++;
		}
	}

	// 만약 보석을 하나도 안 넣었다면 바로 활성화 (예외처리)
	if (CurrentGemCount == 0)
	{
		CheckGemStatus(nullptr);
	}
}

void AGemStatue::CheckGemStatus(AActor* DestroyedGem)
{
	if (bIsActivated) return;

	// 파괴된 녀석은 이미 카운트에서 제외된 셈 치거나, 
	// 현재 살아있는 녀석을 다시 세거나 할 수 있음.
	// 여기서는 OnDestroyed가 호출된 시점이므로 카운트를 하나 줄임.
	if (DestroyedGem)
	{
		CurrentGemCount--;
	}

	// 모든 보석이 파괴됨
	if (CurrentGemCount <= 0)
	{
		bIsActivated = true;

		// 1. 숨겨진 메쉬 등장
		if (HiddenAddonMesh)
		{
			HiddenAddonMesh->SetHiddenInGame(false);
		}

		// 2. 중앙 석상 오른쪽 잔 점화
		if (CenterStatue)
		{
			CenterStatue->ActivateRightGoblet();
		}

		UE_LOG(LogTemp, Warning, TEXT("Right Statue Activated! Gems Cleared!"));
	}
}