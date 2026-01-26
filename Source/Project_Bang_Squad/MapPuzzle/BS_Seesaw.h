#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BS_Seesaw.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API ABS_Seesaw : public AActor
{
	GENERATED_BODY()

public:
	ABS_Seesaw();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* SceneRoot;

	/** 모든 회전의 중심 (자식들을 한꺼번에 돌림) */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* PivotRoot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UStaticMeshComponent* SeesawMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	class UArrowComponent* RotationAxisGuide;

	/* ===== 시소 물리 설정 ===== */
	UPROPERTY(EditAnywhere, Category = "Seesaw|Settings")
	float MaxAngle = 70.f; // 최대 70도까지

	UPROPERTY(EditAnywhere, Category = "Seesaw|Settings")
	float RotationSpeedMultiplier = 0.1f; // 멀리 있을수록 얼마나 빨리 돌아갈지

	UPROPERTY(EditAnywhere, Category = "Seesaw|Settings")
	float ReturnSpeed = 30.f; // 아무도 없을 때 수평으로 돌아오는 속도

private:
	/** 서버에서 계속 갱신되어 복제되는 현재 각도 */
	UPROPERTY(Replicated)
	float ServerCurrentRoll = 0.f;

	// 클라이언트에서 부드러운 화면 출력을 위한 보간용
	float ClientDisplayRoll = 0.f;
};