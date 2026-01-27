// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MagicGate.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API AMagicGate : public AActor
{
	GENERATED_BODY()
	
public:	
	AMagicGate();
	virtual void Tick(float DeltaTime) override;
	
	// 외부(솥)에서 호출할 함수
	UFUNCTION(BlueprintCallable)
	void OpenGate();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UStaticMeshComponent* MeshComp;
	
	// 문이 열렸는지 상태 확인
	bool bIsOpen = false;
	
	// 문이 이동할 목표 높이 (Z축)
	float TargetZ;
	
	// 문이 열릴때 이동할 거리 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate Setting")
	float OpenDistance = 400.0f;

};
