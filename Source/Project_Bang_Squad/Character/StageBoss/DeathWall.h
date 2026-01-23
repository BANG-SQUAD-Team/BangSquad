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

    // 변수 동기화 등록
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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

    // [New] 이동 방향을 에디터에서 직관적으로 보기 위한 화살표
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UArrowComponent* DirectionArrow;

    // [설정]
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Settings")
    TSubclassOf<AActor> PlatformClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Settings")
    TSubclassOf<AActor> JumpPadClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Movement")
    float MoveSpeed = 50.0f;

    // [상태] 서버->클라이언트 동기화 (멈춤/가동 상태 공유)
    UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Wall State")
    bool bIsActive = true;

    // [패턴 설정값]
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern Settings")
    float PlatformStickOut = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern Settings")
    float FirstPlatformHeight = 60.0f;

    // 내부 알고리즘용 변수
    float LayerHeight = 150.0f;
    float BranchWidth = 120.0f;
    float BranchProbability = 0.6f;

private:
    // 내부 로직 분리
    void GeneratePlatforms();

    // 플랫폼 스폰 헬퍼 (중복 코드 제거)
    void SpawnPlatformAt(FVector Location);

    // 사용하지 않는 SpawnRow, SpawnCluster는 제거하거나 필요 시 구현
};