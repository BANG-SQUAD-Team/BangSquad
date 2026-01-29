#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Minigame2_Statue.generated.h"

class UBoxComponent;
class UCurveFloat;

UCLASS()
class PROJECT_BANG_SQUAD_API AMinigame2_Statue : public AActor
{
	GENERATED_BODY()

public:
	AMinigame2_Statue();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UStaticMeshComponent* StatueMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UBoxComponent* TriggerBox;

	/* ===== PillarRotate 식 설정 ===== */
	UPROPERTY(EditAnywhere, Category = "Statue|Settings")
	UCurveFloat* FallCurve;

	UPROPERTY(EditAnywhere, Category = "Statue|Settings")
	float FallDuration = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Statue|Settings")
	FRotator FallRotationAmount = FRotator(0.f, 90.f, 0.f);

	/* ===== 동기화 변수 (PillarRotate 복사) ===== */
	UPROPERTY(ReplicatedUsing = OnRep_bIsFalling)
	bool bIsFalling = false;

	UPROPERTY(Replicated)
	float StartTime = -1.f;

	UPROPERTY(Replicated)
	FRotator ReplicatedInitialRotation;

	UFUNCTION()
	void OnRep_bIsFalling();

	void UpdateFallProgress(float Alpha);

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// 석상이 넘어지면서 최종적으로 도달할 위치 오프셋 (상대 좌표)
	UPROPERTY(EditAnywhere, Category = "Statue|Settings")
	FVector FallLocationOffset = FVector::ZeroVector;

	UPROPERTY(Replicated)
	FVector ReplicatedInitialLocation;

	bool bFloorCrushed = false;
};