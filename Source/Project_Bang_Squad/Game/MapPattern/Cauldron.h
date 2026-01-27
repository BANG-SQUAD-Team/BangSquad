// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Cauldron.generated.h"

class AMagicGate;
class UBoxComponent;

UCLASS()
class PROJECT_BANG_SQUAD_API ACauldron : public AActor
{
	GENERATED_BODY()
	
public:	
	ACauldron();

protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* MeshComp;
	
	// 감지용 박스
	UPROPERTY(VisibleAnywhere)
	UBoxComponent* TriggerBox;
	
	// 에디터에서 연결할 문
	UPROPERTY(EditAnywhere, Category = "Puzzle")
	AMagicGate* TargetGate;
	
	// 오버랩 이벤트 함수
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	
};
