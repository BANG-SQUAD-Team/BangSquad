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

    UFUNCTION()
    void OnRep_bIsFalling();

    void StartFalling();

    void ExecuteFall();

public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    class UStaticMeshComponent* MeshComp;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    class UBoxComponent* DetectionBox;

    UPROPERTY(EditAnywhere, Category = "Settings")
    float FallDelay = 1.0f;

    UPROPERTY(ReplicatedUsing = OnRep_bIsFalling)
    bool bIsFalling = false;

private:
    FTimerHandle FallTimerHandle;
};