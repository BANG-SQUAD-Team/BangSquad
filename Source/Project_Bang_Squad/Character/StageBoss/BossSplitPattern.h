// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossSplitPattern.generated.h"

class UTextRenderComponent;
class UBoxComponent;
//결과 통보용
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSplitPatternFinished, bool, bIsSuccess);

UCLASS()
class PROJECT_BANG_SQUAD_API ABossSplitPattern : public AActor
{
	GENERATED_BODY()
	
public:	
	ABossSplitPattern();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

public:	
	//보스쪽에서 바인딩할 이벤트
	UPROPERTY(BlueprintAssignable)
	FOnSplitPatternFinished OnSplitPatternFinished;

	//패턴 시작 함수
	UFUNCTION(BlueprintCallable)
	void ActivatePattern();

protected:
	//A구역 Component
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* ZoneA_Mesh;

	UPROPERTY(VisibleAnywhere)
	UBoxComponent* ZoneA_Trigger;

	UPROPERTY(VisibleAnywhere)
	UTextRenderComponent* ZoneA_Text;

	//B구역 Component
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* ZoneB_Mesh;

	UPROPERTY(VisibleAnywhere)
	UBoxComponent* ZoneB_Trigger;

	UPROPERTY(VisibleAnywhere)
	UTextRenderComponent* ZoneB_Text;

	//로직
	UPROPERTY(ReplicatedUsing = OnRep_UpdateTexts)
	int32 RequiredA;

	UPROPERTY(ReplicatedUsing = OnRep_UpdateTexts)
	int32 RequiredB;

	int32 CurrentA = 0;
	int32 CurrentB = 0;

	UFUNCTION()
	void OnRep_UpdateTexts();

	//오버랩 이벤트
	UFUNCTION()
	void OnOverlapUpdate(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnEndOverlapUpdate(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void CheckResult();
};

	

