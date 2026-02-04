// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MageSkill2Rock.generated.h"


class USphereComponent;
class UStaticMeshComponent;
class UNiagaraComponent;
class UParticleSystemComponent;

UCLASS()
class PROJECT_BANG_SQUAD_API AMageSkill2Rock : public AActor
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UParticleSystemComponent* ParticleComp;

public:
	AMageSkill2Rock();
	
	// 충돌 감지 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* OverlapComp;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	FVector ParticleScale = FVector(1.0f);
	
	// 바위 메쉬 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* RockMesh;
	
	// 외부에서 데미지와 시전자를 받아오는 함수
	void InitializeRock(float DamageAmount, APawn* InstigatorPawn);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Effects")
	void OnStartEruption();
	
private:
	float SkillDamage;
	APawn* CasterPawn;
	FTimerHandle EruptTimer;
	
	// 솟아오르며 판정하는 함수
	void Erupt();

};
