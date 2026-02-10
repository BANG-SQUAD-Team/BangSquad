#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CenterStatueManager.generated.h"

class UStaticMeshComponent;
class AEnemySpawner;

UCLASS()
class PROJECT_BANG_SQUAD_API ACenterStatueManager : public AActor
{
	GENERATED_BODY()

public:
	ACenterStatueManager();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UStaticMeshComponent* StatueMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UStaticMeshComponent* LeftFireMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UStaticMeshComponent* RightFireMesh;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Link")
	AEnemySpawner* BossSpawner;

	UFUNCTION(BlueprintCallable)
	void ActivateLeftGoblet();

	UFUNCTION(BlueprintCallable)
	void ActivateRightGoblet();

private:
	// 상태 변수 동기화
	UPROPERTY(ReplicatedUsing = OnRep_LeftActive)
	bool bLeftActive = false;

	UPROPERTY(ReplicatedUsing = OnRep_RightActive)
	bool bRightActive = false;

	UFUNCTION() void OnRep_LeftActive();
	UFUNCTION() void OnRep_RightActive();

	bool bPuzzleCompleted = false;
	void CheckPuzzleCompletion();
};