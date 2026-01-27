#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MushroomSpawner.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API AMushroomSpawner : public AActor
{
	GENERATED_BODY()
    
public: 
	AMushroomSpawner();
	virtual void BeginPlay() override;
	// Tick은 이제 필요 없으므로 선언에서 지워도 됩니다. (있어도 상관은 없음)

protected:
	UPROPERTY(EditAnywhere, Category = "Spawner Setting")
	TSubclassOf<AActor> MushroomBPClass;
    
	UPROPERTY(EditAnywhere, Category = "Spawner Setting")
	float RespawnDelay = 3.0f;
    
	UPROPERTY()
	AActor* CurrentMushroom;
    
	FTimerHandle SpawnTimerHandle;
    
	void SpawnMushroom();

	// 🔥 [추가] 버섯이 파괴될 때 호출될 함수 (UFUNCTION 필수!)
	UFUNCTION()
	void OnMushroomDestroyed(AActor* DestroyedActor);
};