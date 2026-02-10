// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemySpawner.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSpawnerCleared);

class UBoxComponent;
class AEnemyNormal;

UCLASS()
class PROJECT_BANG_SQUAD_API AEnemySpawner : public AActor
{
	GENERATED_BODY()
	
public:	
	AEnemySpawner();

protected:
	virtual void BeginPlay() override;

public:	
	// 스포너 활성화 비활성화
	UFUNCTION(BlueprintCallable)
	void SetSpawnerActive(bool bActive);

	// 웨이브 클리어 시 방송할 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Spawner")
	FOnSpawnerCleared OnSpawnerCleared;
	
protected:
	// 구역
	
	// 스폰 구역 (초록색 박스) - 몬스터가 나오는 위치
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawner")
	UBoxComponent* SpawnVolume;
	
	// 트리거 구역 (노란색 박스) - 플레이어가 여기 들어오면 스폰 시작
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawner")
	UBoxComponent* TriggerVolume;
	
	// 스폰 규칙
	// 생성할 적 클래스
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	TSubclassOf<AEnemyNormal> EnemyClass;
	
	// 스폰 주기
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	float SpawnInterval = 1.5f;
	
	// 최대 유지 가능한 적 개수 (이보다 많으면 생성 중단)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	int32 MaxConcurrentEnemyCount = 10;
	
	// [총 소환 제한 ] 이 횟수만큼 뽑으면 스포너 종료 (0 = 무한)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	int32 TotalSpawnLimit =15;
	
	// 한번에 소환할 마릿수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	int32 SpawnCountAtOnce = 2;
	
	// 겹침 방지 최소 거리
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	float MinSpawnDistance = 250.0f;
	
	// 옵션
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	bool bStartAutomatically = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	bool bKeepSpawningAfterLeave = true;
	
private:
	FTimerHandle SpawnTimerHandle;
	bool bIsActive = false;
	
	// 현재 맵에 살아있는 적 (동시 유지 제한용)
	int32 CurrentAliveCount = 0;
	
	// 지금까지 총 소환한 마릿수 (누적)
	int32 TotalSpawnedCount = 0;
	
	void SpawnEnemy();
	FVector GetRandomPointInVolume();
	
	UFUNCTION()
	void OnEnemyDestroyed(AActor* DestroyedActor);
	
	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	
	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

};
