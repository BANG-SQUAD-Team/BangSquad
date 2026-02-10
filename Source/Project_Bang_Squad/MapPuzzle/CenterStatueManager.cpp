#include "Project_Bang_Squad/MapPuzzle/CenterStatueManager.h"
#include "Project_Bang_Squad/Character/Enemy/EnemySpawner.h" 
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

ACenterStatueManager::ACenterStatueManager()
{
	bReplicates = true; // [필수]

	StatueMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatueMesh"));
	RootComponent = StatueMesh;

	LeftFireMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftFireMesh"));
	LeftFireMesh->SetupAttachment(RootComponent);
	LeftFireMesh->SetHiddenInGame(true);

	RightFireMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightFireMesh"));
	RightFireMesh->SetupAttachment(RootComponent);
	RightFireMesh->SetHiddenInGame(true);
}

void ACenterStatueManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACenterStatueManager, bLeftActive);
	DOREPLIFETIME(ACenterStatueManager, bRightActive);
}

void ACenterStatueManager::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && BossSpawner)
	{
		BossSpawner->SetActorEnableCollision(false); // 서버에서만 충돌 끔
	}
}

void ACenterStatueManager::ActivateLeftGoblet()
{
	if (!HasAuthority() || bLeftActive) return;

	bLeftActive = true;
	OnRep_LeftActive(); // 서버 갱신
	CheckPuzzleCompletion();
}

void ACenterStatueManager::ActivateRightGoblet()
{
	if (!HasAuthority() || bRightActive) return;

	bRightActive = true;
	OnRep_RightActive(); // 서버 갱신
	CheckPuzzleCompletion();
}

void ACenterStatueManager::OnRep_LeftActive()
{
	if (LeftFireMesh) LeftFireMesh->SetHiddenInGame(false);
}

void ACenterStatueManager::OnRep_RightActive()
{
	if (RightFireMesh) RightFireMesh->SetHiddenInGame(false);
}

void ACenterStatueManager::CheckPuzzleCompletion()
{
	if (bLeftActive && bRightActive && !bPuzzleCompleted)
	{
		bPuzzleCompleted = true;
		if (BossSpawner) BossSpawner->SetActorEnableCollision(true);

		UE_LOG(LogTemp, Warning, TEXT("Network Puzzle Completed!"));
	}
}