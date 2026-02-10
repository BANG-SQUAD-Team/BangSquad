#include "Project_Bang_Squad/MapPuzzle/GemStatue.h"
#include "Project_Bang_Squad/MapPuzzle/CenterStatueManager.h"
#include "Net/UnrealNetwork.h"
#include "Components/StaticMeshComponent.h"

AGemStatue::AGemStatue()
{
	bReplicates = true;

	StatueMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatueMesh"));
	RootComponent = StatueMesh;

	HiddenAddonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HiddenAddonMesh"));
	HiddenAddonMesh->SetupAttachment(RootComponent);
	HiddenAddonMesh->SetHiddenInGame(true);
}

void AGemStatue::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGemStatue, bIsActivated);
}

void AGemStatue::BeginPlay()
{
	Super::BeginPlay();

	CurrentGemCount = 0;

	for (AActor* Gem : TargetGems)
	{
		if (IsValid(Gem))
		{
			Gem->OnDestroyed.AddDynamic(this, &AGemStatue::CheckGemStatus);
			CurrentGemCount++;
		}
	}

	if (CurrentGemCount == 0)
	{
		CheckGemStatus(nullptr);
	}
}

void AGemStatue::CheckGemStatus(AActor* DestroyedGem)
{
	if (!HasAuthority() || bIsActivated) return;

	if (DestroyedGem) CurrentGemCount--;

	if (CurrentGemCount <= 0)
	{
		bIsActivated = true;
		OnRep_IsActivated();

		if (CenterStatue) CenterStatue->ActivateRightGoblet();
	}
}

void AGemStatue::OnRep_IsActivated()
{
	if (HiddenAddonMesh) HiddenAddonMesh->SetHiddenInGame(false);
}