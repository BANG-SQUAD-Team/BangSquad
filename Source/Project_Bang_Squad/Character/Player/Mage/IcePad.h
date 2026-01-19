
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IcePad.generated.h"

class UBoxComponent;
class UDecalComponent;

UCLASS()
class PROJECT_BANG_SQUAD_API AIcePad : public AActor
{
	GENERATED_BODY()
	
public:	
	AIcePad();

protected:
	virtual void BeginPlay() override;
	
	// 영역 감지용 박스
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* CollisionBox;
	
	// 바닥에 그려질 얼음 문양
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UDecalComponent* IceDecalComp;
	
	// 감속 비율 
	UPROPERTY(EditAnywhere, Category = "Balance")
	float SlowPercentage = 0.5f;
	
	// 장판 지속 시간
	UPROPERTY(EditAnywhere, Category = "Balance")
	float Duration = 5.0f;
	
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
