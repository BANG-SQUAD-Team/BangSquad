// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MushroomSpawner.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API AMushroomSpawner : public AActor
{
	GENERATED_BODY()
	
public:	
	AMushroomSpawner();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

protected:
	// 에디터에서 "실제로 던져질 물리 버섯(BP)"을 넣어주는 변수
	UPROPERTY(EditAnywhere, Category = "Spawner Setting")
	TSubclassOf<AActor> MushroomBPClass;
	
	// 버섯이 사라지면 몇 초 뒤에 다시 나올지
	UPROPERTY(EditAnywhere, Category = "Spawner Setting")
	float RespawnDelay = 5.0f;
	
	// 현재 생성된 버섯을 감시하는 포인터
	UPROPERTY()
	AActor* CurrentMushroom;
	
	// 리필 타이머 핸들
	FTimerHandle SpawnTimerHandle;
	
	// 버섯 생성 함수
	void SpawnMushroom();
};
