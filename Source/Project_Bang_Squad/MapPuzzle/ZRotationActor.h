#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZRotationActor.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API AZRotationActor : public AActor
{
	GENERATED_BODY()

public:
	AZRotationActor();

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/* ===== 컴포넌트 ===== */

	// [NEW] 회전의 기준점이 될 빈 컴포넌트 (여기가 (0,0,0)입니다)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* DefaultSceneRoot;

	// 충돌 담당 박스
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UBoxComponent* CollisionBox;

	// 눈에 보이는 메쉬
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComponent;

	// 회전 담당
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class URotatingMovementComponent* RotatingMovement;

	/* ===== 설정 값 ===== */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation Settings")
	float RotationSpeed = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation Settings")
	bool bClockwise = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback Settings")
	float PushForce = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback Settings")
	float UpwardForce = 200.0f;

	/* ===== 충돌 함수 ===== */
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};