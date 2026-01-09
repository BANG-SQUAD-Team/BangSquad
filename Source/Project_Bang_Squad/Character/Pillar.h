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

	// 동기화할 변수 등록 함수 (필수)
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

public: 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* PillarMesh;

	UPROPERTY(EditAnywhere, Category = "MageAbility")
	FString InteractionText = TEXT("우클릭 + 마우스 왼쪽으로!");

	UPROPERTY(EditAnywhere, Category = "MageAbility")
	float RequiredMouseDirection = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MageAbility", meta = (MakeEditWidget = true))
	FVector FallDirection = FVector(500.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, Category = "MageAbility")
	float FallForce = 1000000.f;

	// [중요] ReplicatedUsing: 이 변수가 서버에서 바뀌면 클라이언트에서 OnRep_IsFallen 함수가 자동 실행됨
	UPROPERTY(ReplicatedUsing = OnRep_IsFallen, VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bIsFallen = false;

	// 상태 변경 감지 함수 (클라이언트에서 실행됨)
	UFUNCTION()
	void OnRep_IsFallen();

	// 메이지가 호출할 함수 (서버에서만 실행되어야 함)
	void TriggerFall();

	UFUNCTION()
	void OnPillarHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};