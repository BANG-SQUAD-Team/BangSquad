#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BS_Door.h"
#include "BS_Button.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API ABS_Button : public AActor
{
    GENERATED_BODY()

public:
    ABS_Button();

    // 다른 버튼이 내 상태를 확인할 때 사용
    bool IsConditionMet() const { return bIsLocallyActivated; }

protected:
    virtual void BeginPlay() override; // 초기 위치 저장을 위해 추가
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    UStaticMeshComponent* BaseMesh;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    UStaticMeshComponent* SwitchMesh;

    /* ===== 에디터 설정 항목 ===== */
    UPROPERTY(EditInstanceOnly, Category = "Settings")
    class ABS_Door* TargetDoor;

    UPROPERTY(EditInstanceOnly, Category = "Settings")
    int32 RequiredPlayers = 1;

    UPROPERTY(EditInstanceOnly, Category = "Settings")
    TArray<ABS_Button*> PartnerButtons;

    UPROPERTY(EditInstanceOnly, Category = "Settings")
    EDoorAction PressedAction = EDoorAction::TempOpen;

    /** 버튼이 눌렸을 때 내려가는 깊이 (기본값 -10) */
    UPROPERTY(EditAnywhere, Category = "Settings")
    float PressDepth = -10.f;

private:
    void EvaluateTotalCondition();

    UPROPERTY(Replicated)
    int32 CurrentPlayerCount = 0;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    bool bIsLocallyActivated = false;
    float VisualInterpAlpha = 0.f;

    FVector InitialRelativeLocation;

    /** 나를 파트너로 등록한 마스터 버튼들 (자동 탐색됨) */
    UPROPERTY()
    TArray<ABS_Button*> MyMasters;

    /** 내가 마스터인지(파트너가 있는지) 확인 */
    bool IsMaster() const { return PartnerButtons.Num() > 0; }
};