#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CenterStatueManager.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class AEnemySpawner;

UCLASS()
class PROJECT_BANG_SQUAD_API ACenterStatueManager : public AActor
{
	GENERATED_BODY()

public:
	ACenterStatueManager();

protected:
	virtual void BeginPlay() override;

public:
	// =================================================================
	// 컴포넌트
	// =================================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* StatueMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* LeftFireMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* RightFireMesh;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Link")
	AEnemySpawner* BossSpawner;

	// =================================================================
	// 기능 함수
	// =================================================================
	UFUNCTION(BlueprintCallable)
	void ActivateLeftGoblet();

	UFUNCTION(BlueprintCallable)
	void ActivateRightGoblet();

private:
	bool bLeftActive = false;
	bool bRightActive = false;
	bool bPuzzleCompleted = false;

	void CheckPuzzleCompletion();
};