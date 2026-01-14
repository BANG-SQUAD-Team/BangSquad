// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/Game/Stage/StageGameMode.h"

#include "GameFramework/Character.h"

void AStageGameMode::SpawnPlayerCharacter(AController* Controller, EJobType JobType)
{
	//데이터 확인
	if (!Controller || !JobCharacterMap.Contains(JobType)) return;

	//기존에 붙어있던 폰이 있다면 제거
	if (APawn* OldPawn = Controller->GetPawn())
	{
		OldPawn->Destroy();
	}

	//스폰할 위치 찾기(PlayerStart 중 하나)
	AActor* StartSpot = FindPlayerStart(Controller);

	FVector SpawnLocation = FVector::ZeroVector;
	FRotator SpawnRotation = FRotator::ZeroRotator;

	if (StartSpot)
	{
		SpawnLocation = StartSpot->GetActorLocation();
		SpawnRotation = StartSpot->GetActorRotation();
	}

	//소환
	UClass* PawnClass = JobCharacterMap[JobType];
	if (APawn* NewPawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnLocation, SpawnRotation))
	{
		Controller->Possess(NewPawn);
	}
}
