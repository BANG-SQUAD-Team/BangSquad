// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MapPortal.generated.h"

class UTextRenderComponent;
class USphereComponent;

UCLASS()
class PROJECT_BANG_SQUAD_API AMapPortal : public AActor
{
	GENERATED_BODY()

public:
	AMapPortal();

protected:
	virtual void BeginPlay() override;

	//원형 구역
	UPROPERTY(EditAnywhere, Category = "BS|Components")
	TObjectPtr<USphereComponent> TriggerSphere;

	//Mesh
	UPROPERTY(VisibleAnywhere, Category = "BS|Components")
	TObjectPtr<UStaticMeshComponent> PortalMesh;

	//카운트다운 표시용 텍스트
	UPROPERTY(VisibleAnywhere, Category = "BS|Components")
	TObjectPtr<UTextRenderComponent> CountdownText;

	//맵 경로
	UPROPERTY(EditAnywhere, Category = "BS|Map")
	TSoftObjectPtr<UWorld> TargetLevel;

	//Overlap
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	//구역 안의 플레이어
	TSet<AActor*> OverlappingPlayers;

	FTimerHandle TravelTimerHandle;

	//카운트다운 갱신용 타이머
	FTimerHandle CountdownUpdateTimer;
	int32 RemainingTime;

	void StartCountdown();
	void CancelCountdown();
	void ProcessLevelTransition();
	void UpdateCountdownText();

	//Client에게 텍스트 변경을 알림
	UFUNCTION(NetMulticast, Reliable)
	void MulticastUpdateText(const FString& NewText);
};
