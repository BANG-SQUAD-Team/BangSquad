// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/Game/Stage/StageGameMode.h"

#include "StagePlayerController.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Project_Bang_Squad/UI/Lobby/JobSelectWidget.h"

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
	/*AActor* StartSpot = FindPlayerStart(Controller);

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
	}*/

	FTransform SpawnTransform = GetRespawnTransform(Controller);
	FVector SpawnLocation = SpawnTransform.GetLocation();
	FRotator SpawnRotation = SpawnTransform.GetRotation().Rotator();

	// 소환 (충돌 처리 옵션 추가: 겹쳐도 강제 소환)
	UClass* PawnClass = JobCharacterMap[JobType];
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	if (APawn* NewPawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnLocation, SpawnRotation, SpawnParams))
	{
		Controller->Possess(NewPawn);
	}
}

void AStageGameMode::RequestRespawn(AController* Controller)
{
	if (!Controller) return;

	FTimerDelegate TimerDelegate;
	TimerDelegate.BindUObject(this, &AStageGameMode::RespawnPlayerElapsed, Controller);

	FTimerHandle TimerHandle;
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, 3.f, false);

	UE_LOG(LogTemp, Warning, TEXT("[GameMode] 3초 뒤 부활 예정..."));
}

void AStageGameMode::ExecuteRespawn(AController* Controller)
{
	if (!Controller) return;

	//컨트롤러에서 저장해둔 직업 정보를 가져옴
	EJobType JobToSpawn = EJobType::Mage;
	if (AStagePlayerController* StagePC = Cast<AStagePlayerController>(Controller))
	{
		JobToSpawn = StagePC->SavedJobType;
	}

	SpawnPlayerCharacter(Controller, JobToSpawn);
}

void AStageGameMode::RespawnPlayerElapsed(AController* DeadController)
{
	if (!DeadController) return;

	APawn* OldPawn = DeadController->GetPawn();
	if (OldPawn)
	{
		OldPawn->Destroy();
	}

	RestartPlayer(DeadController);
}

FTransform AStageGameMode::GetRespawnTransform(AController* Controller)
{
	UWorld* World = GetWorld();
	if (!World) return FTransform::Identity;

	//살아있는 아군 목록 생성
	TArray<AActor*> AllCharacters;
	UGameplayStatics::GetAllActorsOfClass(World, ABaseCharacter::StaticClass(), AllCharacters);

	TArray<ABaseCharacter*> AlivePlayers;
	for (AActor* Actor : AllCharacters)
	{
		ABaseCharacter* Char = Cast<ABaseCharacter>(Actor);
		if (Char && !Char->IsDead() && Char->GetController() != Controller)
		{
			AlivePlayers.Add(Char);
		}
	}

	if (AlivePlayers.Num() > 0)
	{
		//랜덤 아군 선택
		int32 RandomIndex = FMath::RandRange(0, AlivePlayers.Num() - 1);
		ABaseCharacter* Target = AlivePlayers[RandomIndex];

		FVector TargetLocation = Target->GetActorLocation();

		//아군 주변 랜덤 위치
		FVector RandomOffset = FMath::VRand();
		RandomOffset.Z = 0.f;
		RandomOffset.Normalize();
		FVector SpawnLocation = TargetLocation + (RandomOffset * FMath::RandRange(200.f, 400.f));

		SpawnLocation.Z += 200.f;

		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(Target);

		bool bHit = World->LineTraceSingleByChannel(Hit, SpawnLocation, SpawnLocation- FVector(0, 0, 1000.f), ECC_WorldStatic, Params);
		if (bHit)
		{
			return FTransform(FRotator::ZeroRotator, SpawnLocation);
		}
		else
		{
			return FTransform(FRotator::ZeroRotator, TargetLocation + FVector(0, 0, 300.f));
		}
	}

	//아군 전멸 -> PlayerStart
	AActor* StartSpot = FindPlayerStart(Controller);
	if (StartSpot)
	{
		return StartSpot->GetActorTransform();
	}
	return FTransform::Identity;
}
