#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GemStatue.generated.h"

class ACenterStatueManager;

// 보석 액터를 전방 선언 (실제 구현은 그냥 AActor 상속받고 피격 시 Destroy 호출하면 됨)
class AActor;

UCLASS()
class PROJECT_BANG_SQUAD_API AGemStatue : public AActor
{
	GENERATED_BODY()

public:
	AGemStatue();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

public:
	// =================================================================
	// 컴포넌트
	// =================================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* StatueMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* HiddenAddonMesh;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Link")
	ACenterStatueManager* CenterStatue;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Link")
	TArray<AActor*> TargetGems;

	UFUNCTION()
	void CheckGemStatus(AActor* DestroyedGem);

private:
	UPROPERTY(ReplicatedUsing = OnRep_IsActivated)
	bool bIsActivated = false;

	int32 CurrentGemCount = 0;

	UFUNCTION()
	void OnRep_IsActivated();
};