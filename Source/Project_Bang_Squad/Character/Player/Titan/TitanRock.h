#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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
    // 바위 메시
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* RockMesh;

    // 충돌 처리용 (Sphere Collision)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class USphereComponent* CollisionComp;

    // 데미지
    float Damage = 0.0f;
    AActor* OwnerCharacter = nullptr;

    // 초기화 함수
    void InitializeRock(float InDamage, AActor* InOwner);

private:
    UFUNCTION()
    void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};