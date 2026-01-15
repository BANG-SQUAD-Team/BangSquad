#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FallingPad.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API AFallingPad : public AActor
{
    GENERATED_BODY()

public:
    AFallingPad();

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnBoxOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

public:
    UPROPERTY(VisibleAnywhere, Category = "Components")
    class UStaticMeshComponent* MeshComp;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    class UBoxComponent* DetectionBox;

    UPROPERTY(EditAnywhere, Category = "Settings")
    float FallDelay = 0.1f;

private:
    FTimerHandle FallTimerHandle;

    void StartFalling();
};