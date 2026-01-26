

#include "Project_Bang_Squad/Game/MapPattern/MushroomSpawner.h"
#include "TimerManager.h"


AMushroomSpawner::AMushroomSpawner()
{
	PrimaryActorTick.bCanEverTick = true;
}


void AMushroomSpawner::BeginPlay()
{
	Super::BeginPlay();
	// 게임 시작 시 바로 하나 생성
	SpawnMushroom();
	
}

void AMushroomSpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// 1. 현재 관리 중인 버섯이 사라졌는지 확인 
	if (CurrentMushroom == nullptr || !CurrentMushroom->IsValidLowLevel())
	{
		// 2. 버섯은 없는데, 아직 생성 대기 타이머가 안 돌고 있다면? -> 타이머 시작
		if (!GetWorldTimerManager().IsTimerActive(SpawnTimerHandle))
		{
			GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &AMushroomSpawner::SpawnMushroom, 
				RespawnDelay, false);
		}
	}

}

void AMushroomSpawner::SpawnMushroom()
{
	// 할당된 블루프린트가 없으면 생성 불가
	if (!MushroomBPClass) return;
	
	FActorSpawnParameters SpawnParams;
	// 위치가 좀 겹쳐도 강제로 밀어내며 생성
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	
	// 스포너의 위치에 버섯 생성
	CurrentMushroom = GetWorld()->SpawnActor<AActor>(
		MushroomBPClass,
		GetActorLocation(),
		GetActorRotation(),
		SpawnParams
		);
}