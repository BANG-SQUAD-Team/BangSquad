#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RisingPlatform.generated.h"

class UStaticMeshComponent;
class UCurveFloat;
class UCameraShakeBase;

UCLASS()
class PROJECT_BANG_SQUAD_API ARisingPlatform : public AActor
{
	GENERATED_BODY()

public:
	ARisingPlatform();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComp;

	// 위로 올라갈 높이
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float RiseHeight = 300.0f;

	// 올라가는 속도 곡선 (필수)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	UCurveFloat* RiseCurve;

	// 완료 시 화면 흔들림 효과
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	TSubclassOf<UCameraShakeBase> ArrivalCameraShake;

	// 완료 시 소리 (나중에 사운드 큐 추가하기)
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	// USoundBase* ArrivalSound;

	// 매니저가 호출 (NetMulticast)
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_TriggerRise();

private:
	bool bIsRising = false;
	float CurrentCurveTime = 0.0f;
	float MaxCurveTime = 0.0f;

	FVector StartLocation;
	FVector EndLocation;
};