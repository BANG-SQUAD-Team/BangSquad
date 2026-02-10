#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/TimelineComponent.h" 
#include "CombatStatue.generated.h"

class UBoxComponent;
class AEnemySpawner;
class ACenterStatueManager;
class UCurveFloat;

UCLASS()
class PROJECT_BANG_SQUAD_API ACombatStatue : public AActor
{
	GENERATED_BODY()

public:
	ACombatStatue();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

public:
	// =================================================================
	// 컴포넌트
	// =================================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* StatueMesh;

	// 플레이어 감지용 콜리전
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USphereComponent* TriggerSphere;

	// 서서히 변하는 효과를 위한 타임라인
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UTimelineComponent* MaterialTimeline;

	// =================================================================
	// 설정 및 참조
	// =================================================================
	// 레벨에 배치된 EnemySpawner를 에디터에서 지정
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Link")
	AEnemySpawner* LinkedSpawner;

	// 레벨에 배치된 중앙 석상 관리자
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Link")
	ACenterStatueManager* CenterStatue;

	// 머터리얼 변화 곡선 (0~1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	UCurveFloat* ChangeCurve;

	// 변경할 머터리얼 파라미터 이름 (예: "EmissivePower" 또는 "ChangeAlpha")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	FName MaterialParamName = FName("EmissivePower");

private:

	UPROPERTY(ReplicatedUsing = OnRep_IsActivated)
	bool bIsActivated = false;

	UFUNCTION()
	void OnRep_IsActivated();

	// 머터리얼 인스턴스 (런타임 변경용)
	UPROPERTY()
	UMaterialInstanceDynamic* DynMaterial;

	// =================================================================
	// 함수
	// =================================================================
	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnCombatFinished(); // 스포너가 끝났을 때 호출됨

	UFUNCTION()
	void HandleTimelineProgress(float Value); // 타임라인 진행 중 호출
};