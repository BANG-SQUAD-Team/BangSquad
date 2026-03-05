// [AQTEObject.h]
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AQTEObject.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API AQTEObject : public AActor
{
    GENERATED_BODY()

public:
    AQTEObject();

    // [변수 복제] 네트워크 복제를 위해 반드시 선언해야 함
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
    // [핵심] 낙하 시작 함수 (보스 패턴에서 호출)
    void InitializeFalling(AActor* Target, float Duration);

    // [권한 분리] 서버에서만 실행되어 숫자를 올리는 함수
    void RegisterTap();

    void TriggerSuccess();
    void TriggerFailure();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
    TObjectPtr<UStaticMeshComponent> MeshComp;

    UPROPERTY(EditDefaultsOnly, Category = "FX")
    TObjectPtr<UParticleSystem> ExplosionFX;

    // --- [QTE 로직] ---
    UPROPERTY(ReplicatedUsing = OnRep_CurrentTapCount, BlueprintReadOnly, Category = "QTE|Logic")
    int32 CurrentTapCount = 0;

    // [설계] 목표 횟수 (Data Asset 등으로 관리하면 더 좋음)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QTE|Logic")
    int32 TargetTapCount = 100;//QTE목표횟수 40에서 100으로 상향시킴
    
    // [비주얼 동기화] 서버에서 값이 복제될 때 클라이언트에서 실행될 함수
    UFUNCTION()
    void OnRep_CurrentTapCount();

    // --- [비주얼 설정] ---
    // [추가] 낙하 시작 높이 (단위: cm). 3000 = 30미터 상공에서 시작
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QTE|Visual")
    float DropHeight = 3000.0f;

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayExplosion(bool bIsSuccess);

private:
    bool bIsFalling = false;
    float FallDuration = 5.0f;
    float ElapsedTime = 0.0f;
    FVector StartLocation;
    FVector TargetLocation;
};