#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WebProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class AWebPad; // [수정] WebTrap 대신 WebPad 전방 선언

UCLASS()
class PROJECT_BANG_SQUAD_API AWebProjectile : public AActor
{
	GENERATED_BODY()

public:
	AWebProjectile();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USphereComponent> SphereComp;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	// [수정] 빗나갔을 때 생성할 패드 클래스 (여기에 BP_WebBlock1을 넣으세요)
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	TSubclassOf<AWebPad> WebPadClass;

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};