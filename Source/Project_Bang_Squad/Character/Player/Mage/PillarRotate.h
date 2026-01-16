#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/TimelineComponent.h"
#include "Project_Bang_Squad/Character/Player/Mage/MagicInteractableInterface.h"
#include "PillarRotate.generated.h"

class UCurveFloat;

UCLASS()
class PROJECT_BANG_SQUAD_API APillarRotate : public AActor, public IMagicInteractableInterface
{
	GENERATED_BODY()

public:
	APillarRotate();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USceneComponent* SceneRoot;

	// 기둥 메쉬
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* PillarMesh;

	// 애니메이션 제어용 타임라인
	UPROPERTY(VisibleAnywhere, Category = "Timeline")
	UTimelineComponent* FallTimeline;

	// 에디터에서 할당할 넘어짐 커브 (0.0 ~ 1.0)
	UPROPERTY(EditAnywhere, Category = "Timeline")
	UCurveFloat* FallCurve;

	// 회전 기준축 (예: Y축 (0, 1, 0) 설정 시 앞/뒤로 넘어짐)
	UPROPERTY(EditAnywhere, Category = "PillarSettings")
	FVector RotationAxis = FVector(0.f, 1.f, 0.f);

	// 최종 회전 각도 (90도)
	UPROPERTY(EditAnywhere, Category = "PillarSettings")
	float MaxFallAngle = 90.f;

	// 상태 제어 변수
	bool bIsFalling = false;
	FRotator InitialRotation;

	// 타임라인 업데이트 함수
	UFUNCTION()
	void HandleFallProgress(float Value);

public:
	// IMagicInteractableInterface 구현
	virtual void SetMageHighlight(bool bActive) override;
	virtual void ProcessMageInput(FVector Direction) override;
};