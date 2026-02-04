#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SeaMonsterProjectile.generated.h"

// 전방 선언: 컴파일 속도를 높이고 순환 참조를 방지합니다.
class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

UCLASS()
class PROJECT_BANG_SQUAD_API ASeaMonsterProjectile : public AActor
{
	GENERATED_BODY()

public:
	ASeaMonsterProjectile();

	FORCEINLINE UProjectileMovementComponent* GetProjectileMovement() const { return ProjectileMovement; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* CollisionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProjectileMovementComponent* ProjectileMovement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float KnockbackForce = 2000.0f;

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};