#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/Player/Mage/MagicInteractableInterface.h"
#include "GameFramework/Actor.h"
#include "Stage2MagicBall.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
// ❌ 삭제: class UProjectileMovementComponent; (더 이상 안 씀)

UCLASS()
class PROJECT_BANG_SQUAD_API AStage2MagicBall : public AActor, public IMagicInteractableInterface
{
    GENERATED_BODY()
    
public: 
    AStage2MagicBall();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

public: 
    // =================================================================
    // 메이지 인터페이스 구현
    // =================================================================
    virtual void SetMageHighlight(bool bActive) override;
    virtual void ProcessMageInput(FVector Direction) override;

    // =================================================================
    // 공 이동 로직
    // =================================================================
    void StartRolling();
    
    // 충돌 처리 (데미지)
    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
       UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
       bool bFromSweep, const FHitResult& SweepResult);
    
protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* BallMesh;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* DamageTrigger;
    
    // ❌ 삭제: UProjectileMovementComponent* MovementComp; (물리 엔진 쓸 거라 필요 없음)
    
    // 상태 체크 (이름을 cpp와 맞춰서 bIsRolling으로 변경)
    bool bIsRolling = false;
    
    // 굴러가는 속도 (물리 힘)
    UPROPERTY(EditAnywhere, Category = "Ball Stats")
    float MoveSpeed = 1000.0f;
    
    // 굴러갈 방향 (월드 기준)
    UPROPERTY(EditAnywhere, Category = "Ball Stats")
    FVector MoveDirection = FVector(0.0f, 1.0f, 0.0f);
    
    UPROPERTY(EditAnywhere, Category = "Ball Stats")
    float DamageAmount = 50.0f;
    
    // 입력 감도
    UPROPERTY(EditAnywhere, Category = "Ball Stats")
    float RequiredInputThreshold = 0.3f;
};