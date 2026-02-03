#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlatformSpawner.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API APlatformSpawner : public AActor
{
	GENERATED_BODY()

public:
	APlatformSpawner();

protected:
	virtual void BeginPlay() override;

public:
	// 진짜 발판 클래스 (에디터에서 지정)
	UPROPERTY(EditAnywhere, Category = "Spawn Settings")
	TSubclassOf<AActor> RealPlatformClass;

	// 함정 발판 클래스 (에디터에서 지정 - 예: BP_BreakingPad)
	UPROPERTY(EditAnywhere, Category = "Spawn Settings")
	TSubclassOf<AActor> TrapPlatformClass;

	// 발판 간격 (가로, 세로 공통 700)
	UPROPERTY(EditAnywhere, Category = "Spawn Settings")
	float Spacing = 700.0f;

	// 생성할 행 개수 (기본 3줄)
	UPROPERTY(EditAnywhere, Category = "Spawn Settings")
	int32 RowCount = 3;

private:
	void SpawnPlatformRow(int32 RowIndex);
};