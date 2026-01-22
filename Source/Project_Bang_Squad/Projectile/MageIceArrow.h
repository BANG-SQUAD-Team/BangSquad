// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Projectile/MageProjectile.h"
#include "MageIceArrow.generated.h"

/**
 * 
 */
class UNiagaraSystem;

UCLASS()
class PROJECT_BANG_SQUAD_API AMageIceArrow : public AMageProjectile
{
	GENERATED_BODY()
	
public:
	AMageIceArrow();
	
protected:
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	UNiagaraSystem* HitImpactVFX;
	
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_SpawnIceVFX(FVector Location, FRotator Rotation);
	
	// 터질 때 소환할 장판 클래스 (BP_IcePad)
	UPROPERTY(EditDefaultsOnly, Category = "Skill")
	TSubclassOf<AActor> IcePadClass;
	
	// 1. 적과 겹쳤을 때 (부모 함수 덮어 쓰기)
	virtual void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult) override;
	
	// 2. 바닥/벽에 부딪혔을 때 (부모 함수 덮어 쓰기)
	virtual void NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, 
	   bool bSelfMoved, FVector HitLocation, 
	   FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;
	
private:
	// 중복 코드를 줄이기 위한 내부 함수
	void SpawnIcePad(FVector SpawnLocation);
};
