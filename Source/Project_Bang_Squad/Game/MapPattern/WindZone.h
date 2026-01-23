#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WindZone.generated.h"

class UBoxComponent;
class UNiagaraComponent;
class UArrowComponent;

UCLASS()
class PROJECT_BANG_SQUAD_API AWindZone : public AActor
{
	GENERATED_BODY()
	
public:	
	
	AWindZone();
	virtual void Tick(float DeltaTime) override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

protected:
	
	//  땅에 있을 때 바람 힘을 몇 배로 할지 (1.0 ~ 10.0 사이 추천)
	UPROPERTY(EditAnywhere, Category = "Wind Setting")
	float GroundFrictionMultiplier = 2.0f;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UArrowComponent* ArrowComp;
	
	// 바람의 영역
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* WindBox;
	
	// 바람 이펙트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UNiagaraComponent* WindVFX;
	
	// 바람의 세기
	UPROPERTY(EditAnywhere, Category = "Wind Setting")
	float WindStrength = 200000.0f;
	
	// 바람 방향 (True면 화살표 방향, False면 반대)
	UPROPERTY(EditAnywhere, Category = "Wind Setting")
	bool bPushForward = true;
	
	// 바람을 막아주는 채널
	UPROPERTY(EditAnywhere, Category = "Wind Setting")
	TEnumAsByte<ECollisionChannel> BlockChannel = ECC_Visibility;

};
