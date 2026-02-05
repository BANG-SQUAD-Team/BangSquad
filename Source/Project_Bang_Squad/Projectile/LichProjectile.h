#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LichProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UNiagaraSystem;
class UNiagaraComponent;

/**
 * [Lich Projectile]
 * - 리치(Stage 2 보스)가 발사하는 마법 구체
 * - 플레이어(Pawn)에 닿으면 데미지를 주고 폭발
 * - 벽에 닿으면 그냥 폭발
 */
UCLASS()
class PROJECT_BANG_SQUAD_API ALichProjectile : public AActor
{
    GENERATED_BODY()

public:
    ALichProjectile();

protected:
    virtual void BeginPlay() override;

    // 캐릭터(플레이어)와 겹쳤을 때 처리
    UFUNCTION()
    void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    // 벽/바닥에 부딪혔을 때 처리
    virtual void NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp,
        bool bSelfMoved, FVector HitLocation, FVector HitNormal,
        FVector NormalImpulse, const FHitResult& Hit) override;

public:
    // --- [Components] ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> SphereComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UProjectileMovementComponent> MovementComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UNiagaraComponent> NiagaraComp; // 구체 이펙트

    // --- [Settings] ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    float Damage = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
    TObjectPtr<UNiagaraSystem> HitVFX; // 타격/폭발 이펙트
};