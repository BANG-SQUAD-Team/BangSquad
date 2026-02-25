#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TitanThrowableActor.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API ATitanThrowableActor : public AActor
{
	GENERATED_BODY()

public:
	ATitanThrowableActor();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components")
	UStaticMeshComponent* MeshComp;

	UFUNCTION(BlueprintNativeEvent, Category = "Titan|Throw")
	void OnThrown(FVector Direction);

	// --- 폭발 관련 에디터 설정 변수들 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Titan|Explosion")
	float ExplosionRadius = 400.f; // 폭발 반경

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Titan|Explosion")
	float ExplosionDamage = 50.f; // 폭발 데미지

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Titan|Explosion")
	float KnockbackForce = 1500.f; // 넉백 파워

	// --- 함수들 ---
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

protected:
	// 던져진 상태인지 확인하여 바로 터지지 않게 방지
	bool bIsThrown = false;
	bool bHasExploded = false;

	// 서버에서 데미지와 넉백을 처리하는 함수
	void Explode();

	// 모든 클라이언트에서 이펙트와 사운드를 재생하는 함수
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayExplosionEffects();
};