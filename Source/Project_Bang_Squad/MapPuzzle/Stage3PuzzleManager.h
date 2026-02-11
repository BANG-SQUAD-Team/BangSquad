// Stage3PuzzleManager.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Stage3PuzzleManager.generated.h"

class ARisingPlatform;

UCLASS()
class PROJECT_BANG_SQUAD_API AStage3PuzzleManager : public AActor
{
	GENERATED_BODY()

public:
	AStage3PuzzleManager();

public:
	// 순서대로 발판 등록
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Link")
	TArray<ARisingPlatform*> PlatformList;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float RiseInterval = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float InitialDelay = 3.0f;

	UFUNCTION(BlueprintCallable)
	void StartSequence();

private:
	FTimerHandle SequenceTimerHandle;
	int32 CurrentIndex = 0;

	UFUNCTION() void BeginRising();
	UFUNCTION() void TriggerNextPlatform();
};