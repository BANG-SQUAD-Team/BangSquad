#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/TimelineComponent.h"
#include "BossSpikeTrap.generated.h"

class UStaticMeshComponent;
class USceneComponent;

UCLASS()
class PROJECT_BANG_SQUAD_API ABossSpikeTrap : public AActor
{
    GENERATED_BODY()

public:
    ABossSpikeTrap();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // --- Components ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> DefaultSceneRoot;

    // 경고 장판 (반투명 원형)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> WarningDecalMesh;

    // 가시 메쉬 (비주얼 용도일 뿐, 충돌 판정 안 함)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> SpikeMesh;

    // --- Gameplay Logic Settings ---
    UPROPERTY(EditAnywhere, Category = "Trap Logic")
    float TrapRadius = 150.0f; // 판정 범위 (반지름)

    UPROPERTY(EditAnywhere, Category = "Trap Logic")
    float WarningDuration = 1.5f; // 폭발 전 대기 시간

    UPROPERTY(EditAnywhere, Category = "Trap Logic")
    float TrapDamage = 30.0f;

    UPROPERTY(EditAnywhere, Category = "Trap Logic")
    float AirborneStrength = 1000.0f; // 에어본 높이 힘

    UPROPERTY(EditAnywhere, Category = "Trap Visuals")
    TObjectPtr<UCurveFloat> SpikeRiseCurve; // 가시 올라오는 연출용 커브

    // --- Internal Logic ---
    FTimeline SpikeTimeline;

    UFUNCTION()
    void HandleSpikeProgress(float Value); // 타임라인 진행

    // 공격 발동 (서버 로직)
    void TriggerTrapLogic();

    // 비주얼 실행 (클라이언트 전파)
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_ActivateVisuals();
};