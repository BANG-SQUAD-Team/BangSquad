#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Boss1_Rampart.generated.h"

// 성벽의 상태를 정의하는 열거형
UENUM(BlueprintType)
enum class ERampartState : uint8
{
    Idle,       // 정지 상태
    Sinking,    // 땅으로 꺼지는 중
    Rising      // 다시 올라오는 중
};

UCLASS()
class PROJECT_BANG_SQUAD_API ABoss1_Rampart : public AActor
{
    GENERATED_BODY()

public:
    ABoss1_Rampart();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* SceneRoot;

    // 이동 설정
    UPROPERTY(EditAnywhere, Category = "Rampart Settings")
    float MoveDuration = 2.0f;          // 이동에 걸리는 시간

    UPROPERTY(EditAnywhere, Category = "Rampart Settings")
    float TargetDepth = 600.0f;         // 하강할 깊이

    // 진동 설정
    UPROPERTY(EditAnywhere, Category = "Rampart Settings")
    float ShakeIntensity = 4.0f;        // 진동의 세기 (좌우 흔들림)

    UPROPERTY(EditAnywhere, Category = "Rampart Settings")
    float ShakeFrequency = 60.0f;       // 진동의 속도

    // 네트워크 동기화를 위한 상태 변수
    UPROPERTY(ReplicatedUsing = OnRep_RampartState)
    ERampartState RampartState = ERampartState::Idle;

    UFUNCTION()
    void OnRep_RampartState();

private:
    FVector StartLocation;              // 시작 위치 (지상)
    FVector EndLocation;                // 목표 위치 (지하)
    float CurrentProgress = 0.0f;       // 0(지상) ~ 1(지하) 사이의 값

    void UpdateMovement(float DeltaTime);

public:
    virtual void Tick(float DeltaTime) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // 외부 트리거에서 호출할 함수 (예: 보스 패턴 시작 시)
    UFUNCTION(BlueprintCallable, Server, Reliable)
    void Server_SetRampartActive(bool bSink);
};