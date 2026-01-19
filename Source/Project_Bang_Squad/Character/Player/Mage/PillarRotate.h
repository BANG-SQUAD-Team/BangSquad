#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Project_Bang_Squad/Character/Player/Mage/MagicInteractableInterface.h"
#include "PillarRotate.generated.h"

class UCurveFloat;
class USphereComponent;
class UArrowComponent;

UCLASS()
class PROJECT_BANG_SQUAD_API APillarRotate : public AActor, public IMagicInteractableInterface
{
	GENERATED_BODY()

public:
	APillarRotate();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* HingeHelper;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UArrowComponent* DirectionHelper;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* PillarMesh;

	UPROPERTY(EditAnywhere, Category = "Timeline")
	UCurveFloat* FallCurve;

	UPROPERTY(EditAnywhere, Category = "PillarSettings")
	float FallDuration = 1.0f;

	UPROPERTY(EditAnywhere, Category = "PillarSettings")
	FVector FallLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "PillarSettings")
	float MaxFallAngle = 90.f;

	UPROPERTY(ReplicatedUsing = OnRep_bIsFalling)
	bool bIsFalling = false;

	float CurrentTime = 0.0f;
	FVector RotationAxis = FVector(0.f, 1.f, 0.f);
	FRotator InitialRotation;
	FVector InitialLocation;

	UFUNCTION()
	void OnRep_bIsFalling();

	void UpdateFallProgress(float Alpha);

public:
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void SetMageHighlight(bool bActive) override;
	virtual void ProcessMageInput(FVector Direction) override;
};