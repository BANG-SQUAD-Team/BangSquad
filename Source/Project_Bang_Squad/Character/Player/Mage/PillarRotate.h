#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Project_Bang_Squad/Character/Player/Mage/MagicInteractableInterface.h"
#include "PillarRotate.generated.h"

class UCurveFloat;
class USphereComponent;
class UArrowComponent;

UCLASS()
class PROJECT_BANG_SQUAD_API APillarRotate : public AActor, public IMagicInteractableInterface
{
	GENERATED_BODY()

public:
	APillarRotate();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/* ===== 컴포넌트 ===== */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* HingeHelper;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UArrowComponent* DirectionHelper;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* PillarMesh;

	/* ===== 설정 및 커브 ===== */
	UPROPERTY(EditAnywhere, Category = "Pillar|Settings")
	UCurveFloat* FallCurve;

	UPROPERTY(EditAnywhere, Category = "Pillar|Settings")
	float FallDuration = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Pillar|Settings")
	FVector FallLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Pillar|Settings")
	float MaxFallAngle = 90.f;

	/* ===== 멀티플레이어 동기화 변수 ===== */
	UPROPERTY(ReplicatedUsing = OnRep_bIsFalling)
	bool bIsFalling = false;

	UPROPERTY(Replicated)
	float StartTime = -1.f; // 서버 절대 시간 기준 시작점

	UPROPERTY(Replicated)
	FVector ReplicatedRotationAxis;

	UPROPERTY(Replicated)
	FRotator ReplicatedInitialRotation;

	UPROPERTY(Replicated)
	FVector ReplicatedInitialLocation;

	UFUNCTION()
	void OnRep_bIsFalling();

	void UpdateFallProgress(float Alpha);

public:
	/* ===== 인터페이스 구현 ===== */
	virtual void SetMageHighlight(bool bActive) override;
	virtual void ProcessMageInput(FVector Direction) override;
};