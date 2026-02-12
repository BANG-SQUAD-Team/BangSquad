#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/TimelineComponent.h"
#include "JumpPad.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UCurveFloat;

UCLASS()
class PROJECT_BANG_SQUAD_API AJumpPad : public AActor
{
	GENERATED_BODY()

public:
	AJumpPad();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> TriggerBox;

	UPROPERTY(EditAnywhere, Category = "Jump Settings")
	float LaunchStrength = 1800.f;

	UPROPERTY(EditAnywhere, Category = "Jump Settings")
	float SurfaceThreshold = 25.f;

	UPROPERTY(EditAnywhere, Category = "Rise Settings")
	float RiseHeight = 300.0f; // 얼마나 올라갈지 (단위: cm)

	UPROPERTY(EditAnywhere, Category = "Rise Settings")
	float RiseSpeed = 5.0f;    // 올라가는 속도

	UPROPERTY(VisibleAnywhere, Category = "Animation")
	TObjectPtr<UTimelineComponent> BounceTimeline;

	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UCurveFloat> BounceCurve;

	UFUNCTION()
	void HandleTimelineProgress(float Value);

	UFUNCTION()
	void OnTimelineFinished();

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayBounceAnimation();

public:
	// 스위치가 호출할 함수
	void TriggerRise();

private:
	FVector TargetLocation;
	bool bIsRising = false;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_StartRise();

	FVector InitialMeshScale;
};