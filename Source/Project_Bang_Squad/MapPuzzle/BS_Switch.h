#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Project_Bang_Squad/Game/InteractionInterface.h"
#include "BS_Door.h" // EDoorAction 사용을 위해 포함
#include "BS_Switch.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API ABS_Switch : public AActor, public IInteractionInterface
{
    GENERATED_BODY()

public:
    ABS_Switch();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // 인터페이스 구현
    virtual void Interact_Implementation(APawn* InstigatorPawn) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_IsActivated();

public:
    UPROPERTY(VisibleAnywhere, Category = "Components")
    UStaticMeshComponent* BaseMesh;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    UStaticMeshComponent* HandleMesh;

    /** 연결된 문 */
    UPROPERTY(EditInstanceOnly, Category = "Settings")
    class ABS_Door* TargetDoor;

    /** 에디터에서 설정할 동작 (기본값: 완전 열림) */
    UPROPERTY(EditInstanceOnly, Category = "Settings")
    EDoorAction SelectedAction = EDoorAction::Open;

private:
    UPROPERTY(ReplicatedUsing = OnRep_IsActivated)
    bool bIsActivated = false;

    UPROPERTY(Replicated)
    float ActivationTime = -1.f;

    FVector InitialHandleLocation;
};