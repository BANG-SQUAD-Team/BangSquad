#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BreakingPad.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API ABreakingPad : public AActor
{
    GENERATED_BODY()

public:
    ABreakingPad();

protected:
    virtual void BeginPlay() override;

    // 물리적 충돌 감지
    UFUNCTION()
    void OnPieceHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

    // 네트워크 동기화: 지목된 컴포넌트만 물리 활성화
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_DropPiece(UStaticMeshComponent* PieceToDrop);

public:
    UPROPERTY(VisibleAnywhere, Category = "Components")
    USceneComponent* SceneRoot;

    UPROPERTY(EditAnywhere, Category = "Settings")
    float DropDelay = 0.1f;
};