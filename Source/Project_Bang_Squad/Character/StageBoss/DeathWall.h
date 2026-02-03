#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeathWall.generated.h"

// [설정] 3번 채널 = KillZone
#define ECC_KillZone ECC_GameTraceChannel3

UCLASS()
class PROJECT_BANG_SQUAD_API ADeathWall : public AActor
{
    GENERATED_BODY()

public:
    ADeathWall();

protected:
    virtual void BeginPlay() override;
    virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void Tick(float DeltaTime) override;

    UFUNCTION(BlueprintCallable)
    void ActivateWall();

    UFUNCTION(BlueprintCallable)
    void DeactivateWall();

protected:
    // 1. [Root] 킬존 감지용 트리거 (대장)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UBoxComponent* KillZoneTrigger;

    // 2. 물리 벽 (자식)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UStaticMeshComponent* WallMesh;

    // 3. 스폰 볼륨 (자식)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UBoxComponent* SpawnVolume;

    // [설정] 메쉬 회전값
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Settings")
    FRotator MeshRotation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Settings")
    TSubclassOf<AActor> PlatformClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Settings")
    TSubclassOf<AActor> JumpPadClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Movement")
    float MoveSpeed = 50.0f;

    bool bIsActive = true;

    // [패턴 변수들]
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern Settings")
    float FirstPlatformHeight = 60.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern Settings")
    float LayerHeight = 150.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern Settings")
    float PlatformStickOut = 150.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern Settings")
    float RandomFlatProb = 0.3f;

    float BranchWidth = 120.0f;
    float BranchProbability = 0.6f;

    UPROPERTY()
    TArray<AActor*> SpawnedActors;

    void GeneratePlatforms();
};