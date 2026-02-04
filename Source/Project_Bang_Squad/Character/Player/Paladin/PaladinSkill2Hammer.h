// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PaladinSkill2Hammer.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UNiagaraSystem;
class UParticleSystem;        
class UParticleSystemComponent;

UCLASS()
class PROJECT_BANG_SQUAD_API APaladinSkill2Hammer : public AActor
{
	GENERATED_BODY()
	
public:	
	APaladinSkill2Hammer();

protected:
	virtual void BeginPlay() override;
	// 터가 사라질 때 뒷정리하는 함수
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	
	void InitializeHammer(float InDamage, AActor* InCaster);
	
protected:
	// 충돌 시 호출될 함수
	UFUNCTION()
	void OnHammerHit(UPrimitiveComponent* HitComp, AActor* OtherActor, 
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	
	
	// 폭발 로직
	void Explode();
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayImpactVFX(FVector Location);
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_StartLingeringEffect(FVector Location);
	
	// 1. 충돌체 (땅 감지용)
	UPROPERTY(VisibleAnywhere, Category = "Components")
	USphereComponent* CollisionComp;
	
	// 2. 망치 메쉬
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UStaticMeshComponent* HammerMesh;
	
	// 3. 떨어지는 움직임 (투사체 컴포넌트 활용)
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UProjectileMovementComponent* MovementComp;
	
	// 4. 폭발 이펙트
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	UNiagaraSystem* ImpactVFX;
	
	// 이펙트 크기 조절 변수
	UPROPERTY(EditAnywhere, Category = "VFX")
	FVector ImpactVFXScale = FVector(3.0f);
	
	// 설정값
	float Damage = 70.0f;
	AActor* CasterActor = nullptr;
	
	// 폭발 범위
	UPROPERTY(EditAnywhere, Category = "Stats")
	float ExplosionRadius = 600.0f;
	
	// 폭발 후 남는 잔류 이펙트
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	UParticleSystem* LingeringVFX;
	
	// 잔류 이펙트 크기 조절 변수
	UPROPERTY(EditAnywhere, Category = "VFX")
	FVector LingeringVFXScale = FVector(1.0f);
	
	// 잔류 이펙트가 얼마나 유지될지
	UPROPERTY(EditAnywhere, Category = "VFX")
	float LingeringDuration = 3.0f;

	// 실제로 생성된 잔류 이펙트 컴포넌트 (나중에 끄기 위해 저장)
	UPROPERTY()
	UParticleSystemComponent* LingeringVFXComp;
};
