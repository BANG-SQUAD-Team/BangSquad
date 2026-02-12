// Source/Project_Bang_Squad/Character/MonsterBase/WebProjectile.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WebProjectile.generated.h"

// 전방 선언 (컴파일 속도 향상 및 순환 참조 방지)
class USphereComponent;
class UProjectileMovementComponent;
class AWebPad;
class ACocoon;

UCLASS()
class PROJECT_BANG_SQUAD_API AWebProjectile : public AActor
{
	GENERATED_BODY()

public:
	AWebProjectile();

protected:
	// [에러 해결 1] BeginPlay가 선언되어 있어야 CPP에서 구현 가능합니다.
	virtual void BeginPlay() override;

	// [에러 해결 2] 컴포넌트 변수 선언
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* SphereComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComp;

	// CPP에서 ProjectileMovement라고 쓰고 있으므로 이름 통일
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProjectileMovementComponent* ProjectileMovement;

	// [에러 해결 3] 스폰할 클래스 변수 선언
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	TSubclassOf<class AWebPad> WebPadClass;

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	TSubclassOf<class ACocoon> CocoonClass;

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

};