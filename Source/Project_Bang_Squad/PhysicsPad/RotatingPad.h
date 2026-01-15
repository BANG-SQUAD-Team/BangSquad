#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RotatingPad.generated.h"

UENUM(BlueprintType)
enum class ERotateState : uint8
{
    Rotating,
    Waiting
};

UCLASS()
class PROJECT_BANG_SQUAD_API ARotatingPad : public AActor
{
    GENERATED_BODY()

public:
    ARotatingPad();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    class UStaticMeshComponent* MeshComp;

    UPROPERTY(EditAnywhere, Category = "Settings")
    float RotateDuration = 0.4f;

    UPROPERTY(EditAnywhere, Category = "Settings")
    float StayDuration = 2.0f;

private:
    float Timer = 0.0f;
    bool bTargetingMax = true;
    ERotateState CurrentState = ERotateState::Rotating;

    void UpdateRotation(float Alpha);
};