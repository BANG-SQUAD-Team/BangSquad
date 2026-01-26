#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeathWall.generated.h" // 이 파일은 반드시 마지막 include여야 합니다.

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
    // [수정] 메쉬를 담을 루트 컴포넌트 추가
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class USceneComponent* DefaultSceneRoot;

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

    // CPP 생성자용 변수
    float BranchWidth = 120.0f;
    float BranchProbability = 0.6f;

    // 내부 함수
    void GeneratePlatforms();
};