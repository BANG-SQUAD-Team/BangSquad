#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Engine/DataTable.h" 
#include "Project_Bang_Squad/Character/Player/Mage/MagicInteractableInterface.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "InputActionValue.h" // [필수] FInputActionValue 사용 위해 추가
#include "MageCharacter.generated.h"

class UTimelineComponent;
class APillar; //  기둥 전용 로직을 위해 필요
class UCurveFloat; 

UCLASS()
class PROJECT_BANG_SQUAD_API AMageCharacter : public ABaseCharacter
{
    GENERATED_BODY()

public:
    AMageCharacter();

    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    
    // [보트용] 탑다운 카메라
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    USpringArmComponent* TopDownSpringArm;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    UCameraComponent* TopDownCamera;
    
    // [기둥용] 타임라인
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Timeline")
    UTimelineComponent* CameraTimelineComp;

    UPROPERTY(EditAnywhere, Category = "Timeline")
    UCurveFloat* CameraCurve;
    
    // =========================================================
    // 네트워크 (RPC)
    // =========================================================
    
    UFUNCTION(Server, Reliable)
    void Server_ProcessSkill(FName SkillRowName);
    
    UFUNCTION(Server, Reliable)
    void Server_PlayMontage(UAnimMontage* MontageToPlay);
    
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayMontage(UAnimMontage* MontageToPlay);

    //  범용 상호작용 RPC (기둥 파괴, 보트 이동 모두 처리)
    UFUNCTION(Server, Reliable)
    void Server_InteractActor(AActor* TargetActor, FVector Direction);
    
    // 기존 기둥 파괴 RPC (삭제해도 되지만, 기둥 로직 보존을 위해 남겨둠)
    UFUNCTION(Server, Reliable)
    void Server_TriggerPillarFall(APillar* TargetPillar);

protected:
    virtual void BeginPlay() override;
    virtual void OnDeath() override;
    
    virtual void Attack() override;
    virtual void Skill1() override;
    virtual void Skill2() override;
    
    //  Look 함수 (override 제거하고 독자 함수로 선언)
    void Look(const FInputActionValue& Value) override;
    
    // 콤보 및 스킬 관련 변수
    int32 CurrentComboIndex = 0;
    FTimerHandle ComboResetTimer;
    void ResetCombo();

    FTimerHandle ProjectileTimerHandle;
    void SpawnDelayedProjectile(UClass* ProjectileClass);

    // 직업 능력 (Job Ability)
    virtual void JobAbility() override;
    void EndJobAbility();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data")
    UDataTable* SkillDataTable;

private:
    UFUNCTION()
    void CameraTimelineProgress(float Alpha);

    UFUNCTION()
    void OnCameraTimelineFinished();

    float DefaultArmLength;
    FVector DefaultSocketOffset;
    
    void ProcessSkill(FName SkillRowName);
    
    // 범용 타겟 감지 함수로 통합
    void CheckInteractableTarget();

    // 기둥 전용 락온 로직
    void LockOnPillar(float DeltaTime);
    
    UPROPERTY(EditAnywhere, Category = "Telekinesis")
    float TraceDistance = 5000.f;

    //  기둥 관련 변수
    UPROPERTY()
    APillar* FocusedPillar;
    UPROPERTY()
    APillar* CurrentTargetPillar;

    // [추가] 보트(범용) 관련 변수
    UPROPERTY()
    AActor* HoveredActor; // 마우스 오버된 대상
    UPROPERTY()
    AActor* CurrentControlledActor; // 조종 중인 대상

    // [추가] 상태 플래그
    bool bIsPillarMode = false;
    bool bIsBoatMode = false;
};