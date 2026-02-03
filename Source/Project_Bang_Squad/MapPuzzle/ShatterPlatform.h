#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShatterPlatform.generated.h"

class UGeometryCollectionComponent;
class UBoxComponent;

UCLASS()
class PROJECT_BANG_SQUAD_API AShatterPlatform : public AActor
{
	GENERATED_BODY()

public:
	AShatterPlatform();

protected:
	virtual void BeginPlay() override;

public:
	// 카오스 파괴를 위한 지오메트리 컬렉션 (발판 메쉬)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UGeometryCollectionComponent* GCComponent;

	// 플레이어 감지용 박스
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* TriggerBox;

	// 부서지기까지 필요한 플레이어 수 (기본 4)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shatter Settings")
	int32 RequiredPlayerCount = 4;

	// 부서진 후 사라지는 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shatter Settings")
	float CleanupDelay = 10.0f;

	// 파괴 강도 (Strain)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shatter Settings")
	float ShatterDamage = 500000.0f;

private:
	// 오버랩 이벤트 처리 함수
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// 현재 올라와 있는 플레이어 수 확인
	void CheckPlayerCount();

	// 이미 부서졌는지 체크
	bool bIsShattered = false;

	// 모든 클라이언트에게 파괴 명령
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Shatter();
};