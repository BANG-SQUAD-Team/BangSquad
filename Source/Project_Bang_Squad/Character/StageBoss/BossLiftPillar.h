#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "BossLiftPillar.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API ABossLiftPillar : public AActor
{
	GENERATED_BODY()

public:
	ABossLiftPillar();

	/** * @param TargetHeight : 최종 높이
	 * @param RiseDuration : 상승 시간
	 * @param LifeTimeOffset : 상승 후 유지 시간
	 */
	void InitializeLift(float TargetHeight, float RiseDuration, float LifeTimeOffset);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> LiftMovement;
};