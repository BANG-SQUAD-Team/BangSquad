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

protected:
    // 벽 메쉬
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UStaticMeshComponent* WallMesh;

    // 발판 생성 구역 박스
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UBoxComponent* SpawnVolume;

    // 발판 클래스
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Settings")
    TSubclassOf<AActor> PlatformClass;

    // 벽 이동 속도
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Settings")
    float MoveSpeed = 100.0f;

    // [트리 시스템 설정값]

    // 첫 발판 높이 (바닥에서 60cm)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Settings")
    float FirstPlatformHeight = 60.0f;

    // 층 높이 (캐릭터 키 80 + 점프 여유 고려 -> 150 추천)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Settings")
    float LayerHeight = 150.0f;

    // 가지가 뻗어나가는 좌우 간격
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Settings")
    float BranchWidth = 120.0f;

    // [중요] 벽에서 튀어나오는 정도 (이 변수가 없어서 에러가 났었습니다)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Settings")
    float PlatformStickOut = 150.0f;

    // [중요] 발판끼리의 최소 좌우 간격 (이 값보다 가까우면 삭제)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Settings")
    float MinHorizontalGap = 60.0f;

    // 가지치기 확률 (0.0 ~ 1.0)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Settings")
    float BranchProbability = 0.6f;

    bool bIsActive = false;
    void GeneratePlatforms();

    void SpawnRow(float Z, int32 CenterIdx, int32 Size, float Width, FVector Origin, FVector Fwd, FVector Right);

    void SpawnCluster(float Z, int32 CenterIdx, int32 Size, float Width, FVector Origin, FVector Fwd, FVector Right);
};