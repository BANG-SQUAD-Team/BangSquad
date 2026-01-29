#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SeaMonsterProjectile.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API ASeaMonsterProjectile : public AActor
{
	GENERATED_BODY()

public:
	ASeaMonsterProjectile();

	FORCEINLINE class UProjectileMovementComponent* GetProjectileMovement() const { return ProjectileMovement; }

protected:
	// 충돌체 (이게 루트)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USphereComponent* CollisionComp;

	/** [NEW] 공의 외형을 담당할 메쉬 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* MeshComp;

	// 발사체 이동 로직
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UProjectileMovementComponent* ProjectileMovement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float KnockbackForce = 2000.0f;

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};