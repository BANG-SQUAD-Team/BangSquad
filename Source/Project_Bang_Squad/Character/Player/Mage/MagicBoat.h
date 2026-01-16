// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MagicInteractableInterface.h"
#include "MagicBoat.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API AMagicBoat : public AActor, public IMagicInteractableInterface
{
	GENERATED_BODY()
	
public:	
	AMagicBoat();

protected:
	virtual void BeginPlay() override;

	FVector CurrentMoveDirection = FVector::ZeroVector;
public:
	virtual void Tick(float DeltaTime) override;
	
	// 보트 외형
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComp;

	// 이동 속도
	UPROPERTY(EditAnywhere, Category = "Stats")
	float MoveSpeed = 4000.0f;

	// [인터페이스 함수 선언]
	//1. 아웃라인 켜기/끄기
	virtual void SetMageHighlight(bool bActive) override;
	//2. 메이지의 마법 입력 처리 (보트는 이동으로 반응)
	virtual void ProcessMageInput(FVector Direction) override;
	

};
