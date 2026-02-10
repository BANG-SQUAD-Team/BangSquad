#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LavaPad.generated.h"

class UStaticMeshComponent;
// [변경] ACharacter -> ABaseCharacter (님의 프로젝트 베이스 캐릭터)
class ABaseCharacter;

UCLASS()
class PROJECT_BANG_SQUAD_API ALavaPad : public AActor
{
	GENERATED_BODY()

public:
	ALavaPad();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* FloorMesh;

	// =================================================================
	// 설정 변수
	// =================================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lava Settings")
	float DamageAmount = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lava Settings")
	float DamageInterval = 0.5f;

	// [변경] 직접 속도값이 아니라, ApplySlowDebuff에 넘길 배율 (0.5 = 50% 느려짐)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lava Settings")
	float SpeedMultiplier = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lava Settings")
	float BurnDurationAfterExit = 3.0f;

private:
	FTimerHandle DamageTimerHandle;
	FTimerHandle ExtinguishTimerHandle;

	// 원래 수치 복구용 변수들
	float OriginalGroundFriction;
	float OriginalBrakingDeceleration;

	UPROPERTY()
	ABaseCharacter* TargetCharacter; // [변경] ABaseCharacter로 저장

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION() void ApplyBurnDamage();
	UFUNCTION() void StopBurn();
};