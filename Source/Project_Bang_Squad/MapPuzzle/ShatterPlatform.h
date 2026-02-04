#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShatterPlatform.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API AShatterPlatform : public AActor
{
	GENERATED_BODY()

public:
	AShatterPlatform();

protected:
	virtual void BeginPlay() override;

public:
	// 깨지는 돌멩이 (비주얼 + 파편용)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UGeometryCollectionComponent* GCComponent;

	// 감지 구역 (몇 명 왔나?)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UBoxComponent* TriggerBox;

	// [추가됨] 플레이어가 실제로 밟고 서 있을 '투명 바닥' (큐브)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UBoxComponent* MainFloorCollision;

	// 설정값들
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shatter Settings")
	int32 RequiredPlayerCount = 1; // 테스트용으로 1로 설정

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shatter Settings")
	float ShatterDamage = 500.0f; // 돌의 체력(Threshold)보다 높아야 함

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shatter Settings")
	float CleanupDelay = 3.0f; // 3초 뒤 삭제

	// 상태 변수
	bool bIsShattered = false;

	// 함수들
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void CheckPlayerCount();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Shatter();
};