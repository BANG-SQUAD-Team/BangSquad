#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/TimelineComponent.h"
#include "MovingPad.generated.h"

class UStaticMeshComponent;
class UCurveFloat;

UCLASS()
class PROJECT_BANG_SQUAD_API AMovingPad : public AActor
{
	GENERATED_BODY()

public:
	AMovingPad();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Animation")
	TObjectPtr<UTimelineComponent> MoveTimeline;

	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UCurveFloat> MoveCurve;

	UPROPERTY(EditAnywhere, Category = "Settings", meta = (MakeEditWidget = true))
	FVector EndLocation;

	UFUNCTION()
	void HandleMoveProgress(float Value);

private:
	FVector StartLocation;
};