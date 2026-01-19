#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeathWall.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API ADeathWall : public AActor
{
    GENERATED_BODY()

public:
    ADeathWall();
    virtual void Tick(float DeltaTime) override;

protected:
    virtual void BeginPlay() override;

    // --- [Components] ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UBoxComponent* KillZone; // 닿으면 죽는 범위

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* WallMesh; // 벽의 형태

    // --- [Settings] ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathWall")
    float MoveSpeed = 100.0f; // 벽의 이동 속도

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathWall")
    bool bIsActive = false; // 활성화 여부

    // 즉사 데미지 양
    UPROPERTY(EditAnywhere, Category = "DeathWall")
    float KillDamage = 9999.0f;

    // --- [Events] ---
    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

public:
    // 외부(보스)에서 벽을 출발시키는 함수
    UFUNCTION(BlueprintCallable, Category = "DeathWall")
    void ActivateWall();
};