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

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    UFUNCTION(BlueprintCallable)
    void ActivateWall();

    UFUNCTION(BlueprintCallable)
    void DeactivateWall();

protected:
    // [컴포넌트]
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UStaticMeshComponent* WallMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UBoxComponent* SpawnVolume;

    // [설정]
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Settings")
    TSubclassOf<AActor> PlatformClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Settings")
    TSubclassOf<AActor> JumpPadClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Movement")
    float MoveSpeed = 50.0f;

    bool bIsActive = true;

    // [패턴 설정값]
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern Settings")
    float FirstPlatformHeight = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern Settings")
    float LayerHeight = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern Settings")
    float PlatformStickOut = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern Settings")
    float RandomFlatProb = 0.3f;

    // CPP 생성자에 있어서 추가함 (컴파일 에러 방지용)
    float BranchWidth = 120.0f;
    float BranchProbability = 0.6f;

    // 내부 함수
    void GeneratePlatforms();
    void SpawnRow(float Z, int32 CenterIdx, int32 Size, float Width, FVector Origin, FVector Fwd, FVector Right);
    void SpawnCluster(float Z, int32 CenterIdx, int32 Size, float Width, FVector Origin, FVector Fwd, FVector Right);
};