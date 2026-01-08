#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Pillar.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API APillar : public AActor
{
	GENERATED_BODY()
	
public:	
	APillar();

protected:
	virtual void BeginPlay() override;

public:	
	// 기둥의 메쉬
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* PillarMesh;

	// 메이지가 이 기둥을 봤을 때 띄울 안내 문구
	UPROPERTY(EditAnywhere, Category = "MageAbility")
	FString InteractionText = TEXT("우클릭 + 마우스 왼쪽으로!");

	// 넘어뜨리기 위해 필요한 마우스 방향 (-1: 왼쪽, 1: 오른쪽)
	UPROPERTY(EditAnywhere, Category = "MageAbility")
	float RequiredMouseDirection = -1.0f;

	// 넘어지는 방향 (에디터에서 화살표로 조절 가능)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MageAbility", meta = (MakeEditWidget = true))
	FVector FallDirection = FVector(500.f, 0.f, 0.f);

	// 넘어지는 힘의 세기
	UPROPERTY(EditAnywhere, Category = "MageAbility")
	float FallForce = 1000000.f;

	bool bIsFallen = false;

	// 메이지가 기둥을 넘어뜨리는 함수
	void TriggerFall();

	// 바닥에 닿았을 때 실행될 함수
	UFUNCTION()
	void OnPillarHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};