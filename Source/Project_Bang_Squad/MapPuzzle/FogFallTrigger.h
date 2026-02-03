#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/Light.h"
#include "Components/TimelineComponent.h" // 타임라인 필수 헤더
#include "FogFallTrigger.generated.h"

class UBoxComponent;
class AExponentialHeightFog;
class UCurveFloat;

UCLASS()
class PROJECT_BANG_SQUAD_API AFogFallTrigger : public AActor
{
	GENERATED_BODY()

public:
	AFogFallTrigger();

protected:
	virtual void BeginPlay() override;

public:
	// 플레이어 감지용 박스
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UBoxComponent* TriggerBox;

	// 타임라인 컴포넌트
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UTimelineComponent* FogTimeline;

	/* ===== 에디터 설정 변수 ===== */

	// 1. 레벨에 있는 포그
	UPROPERTY(EditAnywhere, Category = "Fog Settings")
	AExponentialHeightFog* TargetFog;

	// 2. 커브 에셋
	UPROPERTY(EditAnywhere, Category = "Fog Settings")
	UCurveFloat* FogCurve;

	// 3. 목표 높이 (도착지점 Z값)
	UPROPERTY(EditAnywhere, Category = "Fog Settings")
	float TargetZHeight = -8053.0f;

	/* ===== [추가] 조명 설정 변수 ===== */

	// 1. 제어할 조명
	UPROPERTY(EditAnywhere, Category = "Light Settings")
	ALight* TargetLight;

	// 2. 목표 밝기
	UPROPERTY(EditAnywhere, Category = "Light Settings")
	float TargetIntensity = 0.04f;

private:
	// 시작할 때의 포그 높이를 저장할 변수
	float StartZHeight;

	// 시작할 때의 밝기 저장용
	float StartIntensity;

	// 타임라인이 틱마다 호출할 함수
	UFUNCTION()
	void UpdateFogHeight(float Value);

	// 박스에 닿았을 때
	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};