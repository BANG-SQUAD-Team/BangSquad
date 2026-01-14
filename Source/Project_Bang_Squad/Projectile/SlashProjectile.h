#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlashProjectile.generated.h"

class UBoxComponent;
class UProjectileMovementComponent;
class UParticleSystemComponent;

UCLASS()
class PROJECT_BANG_SQUAD_API ASlashProjectile : public AActor
{
	GENERATED_BODY()

public:
	ASlashProjectile();

protected:
	virtual void BeginPlay() override;

public:
	// 참격 데미지
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float Damage = 30.0f;

protected:
	// 판정용 박스 (가로로 넓게)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> BoxComp;

	// 이동 담당
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	// 이펙트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UParticleSystemComponent> ParticleComp;

	// 충돌 함수
	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};