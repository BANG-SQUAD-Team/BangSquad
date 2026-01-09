#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MageProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UParticleSystem;
class UParticleSystemComponent;

UCLASS()
class PROJECT_BANG_SQUAD_API AMageProjectile : public AActor
{
	GENERATED_BODY()
    
public: 
	AMageProjectile();

protected:
	virtual void BeginPlay() override;
    
	/** 투사체의 충돌 범위 (Root) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* SphereComp;
    
	/** 투사체의 외형 (Mesh - 필요 없으면 에디터에서 비워도 됨) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComp;

	/** 투사체 이펙트 (파티클) - 마법 효과용 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UParticleSystemComponent* ParticleComp;
    
	/** 투사체의 이동을 제어하는 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProjectileMovementComponent* ProjectileMovement;
    
	/** 무언가에 부딪혔을 때 호출되는 함수 */
	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
	   UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
	   bool bFromSweep, const FHitResult& SweepResult);
    
public: 
	/** 캐릭터(Mage)가 생성할 때 전달해 줄 데미지 값*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	float Damage = 20.0f; 
};