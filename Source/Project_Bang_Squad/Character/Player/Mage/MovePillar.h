#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Project_Bang_Squad/Character/Player/Mage/MagicInteractableInterface.h"
#include "MovePillar.generated.h"

class UCurveFloat;

UCLASS()
class PROJECT_BANG_SQUAD_API AMovePillar : public AActor, public IMagicInteractableInterface
{
	GENERATED_BODY()

public:
	AMovePillar();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// =================================================================
	// 인터페이스 구현
	// =================================================================
	virtual void SetMageHighlight(bool bActive) override;
	virtual void ProcessMageInput(FVector Direction) override;

protected:
	// =================================================================
	// 컴포넌트
	// =================================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* SceneRoot;

	// [수정 2] 여러 개의 메쉬를 관리하기 위해 배열로 변경하지 않고,
	// "이 액터의 자식으로 붙은 모든 스태틱 메쉬"를 움직이는 방식으로 구현합니다.
	// (에디터에서 자유롭게 추가 가능)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* MovingPlatformRoot;

	// =================================================================
	// 설정 변수
	// =================================================================
	// 목표 이동 지점 (현재 위치 기준 상대 좌표)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (MakeEditWidget = true), Category = "MovePillar")
	FVector TargetLocationOffset;

	// 목표까지 도달하는 데 걸리는 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovePillar")
	float MoveDuration = 2.0f;

	// 돌아올 때(낙하) 걸리는 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovePillar")
	float ReturnDuration = 0.5f;

	// 상승 중 진동 세기
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovePillar")
	float VibrationIntensity = 2.0f;

	// 낙하 시 사용할 커브
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovePillar")
	UCurveFloat* FallCurve;

	// =================================================================
	// 상태 변수
	// =================================================================
	UPROPERTY(ReplicatedUsing = OnRep_IsActive)
	bool bIsActive = false;

	UFUNCTION()
	void OnRep_IsActive();

	FVector StartLocation;
	FVector TargetLocation;

	float CurrentAlpha = 0.0f;
};