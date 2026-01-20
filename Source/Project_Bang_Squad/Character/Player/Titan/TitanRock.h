#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h" // [중요] 이게 없으면 BeginPlay를 못 찾습니다.
#include "TitanRock.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API ATitanRock : public AActor
{
	GENERATED_BODY()

public:
	ATitanRock();

protected:
	virtual void BeginPlay() override;

public:
	// 초기화 함수
	void InitializeRock(float InDamage, AActor* InOwner);

	// 바위 메시
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* RockMesh;

	// 충돌 처리용 (Sphere Collision)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USphereComponent* CollisionComp;

	// 설정값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rock Stat")
	float ExplosionRadius = 350.0f; // 폭발 범위

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rock Stat")
	float KnockbackForce = 700.0f; // 밀쳐내는 힘

private:
	// 내부 변수
	float Damage = 0.0f;

	UPROPERTY()
	AActor* OwnerCharacter = nullptr;

	// 충돌 감지 함수
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	// 폭발 로직 함수
	void Explode();
};