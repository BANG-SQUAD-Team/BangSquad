// [AQTEObject.h]
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AQTEObject.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API AQTEObject : public AActor
{
    GENERATED_BODY()

public:
    AQTEObject();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

public:
    // [핵심] 보스 클래스에서 호출할 함수들 선언
    void InitializeFalling(AActor* Target, float Duration);
    void TriggerSuccess();
    void TriggerFailure();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
    TObjectPtr<UStaticMeshComponent> MeshComp;

    UPROPERTY(EditDefaultsOnly, Category = "FX")
    TObjectPtr<UParticleSystem> ExplosionFX;

    // [비주얼 동기화]
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayExplosion(bool bIsSuccess);

private:
    bool bIsFalling = false;
    float FallDuration = 5.0f;
    float ElapsedTime = 0.0f;
    FVector StartLocation;
    FVector TargetLocation;
};