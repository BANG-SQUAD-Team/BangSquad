#include "Project_Bang_Squad/MapPuzzle/CenterStatueManager.h"
#include "Project_Bang_Squad/Character/Enemy/EnemySpawner.h"
#include "Components/StaticMeshComponent.h"

ACenterStatueManager::ACenterStatueManager()
{
	PrimaryActorTick.bCanEverTick = false;

	// 1. 메인 석상 메쉬
	StatueMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatueMesh"));
	RootComponent = StatueMesh;

	// [수정] 2. 왼쪽 불꽃 메쉬
	LeftFireMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftFireMesh"));
	LeftFireMesh->SetupAttachment(RootComponent);
	LeftFireMesh->SetCollisionProfileName(TEXT("NoCollision"));
	LeftFireMesh->SetHiddenInGame(true);
	LeftFireMesh->SetCastShadow(false);

	// [수정] 3. 오른쪽 불꽃 메쉬
	RightFireMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightFireMesh"));
	RightFireMesh->SetupAttachment(RootComponent);
	RightFireMesh->SetCollisionProfileName(TEXT("NoCollision"));
	RightFireMesh->SetHiddenInGame(true);
	RightFireMesh->SetCastShadow(false);
}

void ACenterStatueManager::BeginPlay()
{
	Super::BeginPlay();

	if (BossSpawner)
	{
		BossSpawner->SetActorEnableCollision(false);
	}
}

void ACenterStatueManager::ActivateLeftGoblet()
{
	if (bLeftActive) return;
	bLeftActive = true;

	if (LeftFireMesh)
	{
		LeftFireMesh->SetHiddenInGame(false);
	}

	UE_LOG(LogTemp, Warning, TEXT("Left Goblet Ignited (Mesh Visible)!"));
	CheckPuzzleCompletion();
}

void ACenterStatueManager::ActivateRightGoblet()
{
	if (bRightActive) return;
	bRightActive = true;

	if (RightFireMesh)
	{
		RightFireMesh->SetHiddenInGame(false);
	}

	UE_LOG(LogTemp, Warning, TEXT("Right Goblet Ignited (Mesh Visible)!"));
	CheckPuzzleCompletion();
}

void ACenterStatueManager::CheckPuzzleCompletion()
{
	// 좌우 불꽃이 모두 켜졌고, 아직 완료 처리가 안 됐다면
	if (bLeftActive && bRightActive && !bPuzzleCompleted)
	{
		bPuzzleCompleted = true;

		UE_LOG(LogTemp, Warning, TEXT("Puzzle Completed! Activating Boss Spawner Immediately."));

		// [핵심] 트리거 활성화 대신, 보스 스포너를 바로 켜버림
		if (BossSpawner)
		{
			BossSpawner->SetActorEnableCollision(true);
		}
	}
}
