#include "Project_Bang_Squad/MapPuzzle/Stage3PuzzleManager.h"
#include "Project_Bang_Squad/MapPuzzle/RisingPlatform.h"
#include "TimerManager.h"

AStage3PuzzleManager::AStage3PuzzleManager()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void AStage3PuzzleManager::StartSequence()
{
	if (!HasAuthority()) return;

	GetWorldTimerManager().SetTimer(SequenceTimerHandle, this, &AStage3PuzzleManager::BeginRising, InitialDelay, false);
}

void AStage3PuzzleManager::BeginRising()
{
	CurrentIndex = 0;
	TriggerNextPlatform();
	GetWorldTimerManager().SetTimer(SequenceTimerHandle, this, &AStage3PuzzleManager::TriggerNextPlatform, RiseInterval, true);
}

void AStage3PuzzleManager::TriggerNextPlatform()
{
	if (PlatformList.IsValidIndex(CurrentIndex))
	{
		if (ARisingPlatform* Platform = PlatformList[CurrentIndex])
		{
			Platform->Multicast_TriggerRise();
		}
		CurrentIndex++;
	}
	else
	{
		GetWorldTimerManager().ClearTimer(SequenceTimerHandle);
	}
}