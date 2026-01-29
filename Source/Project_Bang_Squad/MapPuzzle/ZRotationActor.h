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
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UStaticMeshComponent* MeshComponent;

	/* ===== 회전 설정 (에디터 수정 가능) ===== */

	/** 초당 회전 속도 (도 단위) */
	UPROPERTY(EditAnywhere, Category = "Rotation Settings")
	float RotationSpeed = 90.0f;

	/** 시계 방향 여부 (체크하면 시계 방향, 해제하면 반시계) */
	UPROPERTY(EditAnywhere, Category = "Rotation Settings")
	bool bClockwise = true;

	/** 부딪혔을 때 밀어내는 힘 */
	UPROPERTY(EditAnywhere, Category = "Knockback Settings")
	float PushForce = 1500.0f;

	/** 살짝 띄워주는 상단 힘 (이게 있어야 잘 날아감) */
	UPROPERTY(EditAnywhere, Category = "Knockback Settings")
	float UpwardForce = 500.0f;

	/** 충돌 감지를 위한 함수 */
	UFUNCTION()
	void OnMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};