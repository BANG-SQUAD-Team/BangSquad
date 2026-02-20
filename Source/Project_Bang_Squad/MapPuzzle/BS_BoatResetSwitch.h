// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Project_Bang_Squad/Game/InteractionInterface.h"
#include "BS_BoatResetSwitch.generated.h"

class AMagicBoat;
class USphereComponent;
class UWidgetComponent;

UCLASS()
class PROJECT_BANG_SQUAD_API ABS_BoatResetSwitch : public AActor, public IInteractionInterface
{
	GENERATED_BODY()
	
public:	
	ABS_BoatResetSwitch();

protected:
	virtual void BeginPlay() override;
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	// 인터페이스 구현 (상호작용시 호출됨)
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	
	// 버튼 상태가 변경될 때 호출 (클라이언트 시각 효과용)
	UFUNCTION()
	void OnRep_IsPressed();
	
	// 일정 시간 후 버튼을 다시 튀어오르게 하는 함수
	void ResetButtonState();
	
	//  3. 오버랩 이벤트 함수 
	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

public:	
	// 스위치 본체 메쉬 (움직이지 않음)
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UStaticMeshComponent* BaseMesh;
	
	// 플레이어들이 소환될 위치
	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* PlayerSpawnPoint; 
	
	// 연결할 보트
	UPROPERTY(EditInstanceOnly, Category = "Settings")
	AMagicBoat* TargetBoat;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* InteractSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UWidgetComponent* InteractWidget;
	
private:
	// 중복 실행 방지용 플래그 (쿨타임)
	UPROPERTY(ReplicatedUsing = OnRep_IsPressed)
	bool bIsPressed;
	
	// 복구 타이머 
	FTimerHandle ButtonResetTimer;
	

};
