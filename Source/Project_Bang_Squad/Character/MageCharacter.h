#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Engine/DataTable.h" 
#include "Project_Bang_Squad/Character/Player/Mage/MagicInteractableInterface.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "InputActionValue.h" 
#include "NiagaraSystem.h" 
#include "NiagaraComponent.h"
#include "Project_Bang_Squad/Character/Player/Mage/PillarRotate.h"
#include "MageCharacter.generated.h"

class UTimelineComponent;
class APillar;
class UCurveFloat; 
class UNiagaraSystem;
class UParticleSystem;

UCLASS()
class PROJECT_BANG_SQUAD_API AMageCharacter : public ABaseCharacter
{
    GENERATED_BODY()

public:
    // ====================================================================================
    //  섹션 1: 생성자 및 필수 오버라이드
    // ====================================================================================
    AMageCharacter();

    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

    // ====================================================================================
    //  섹션 2: 네트워크 및 서버 통신 (RPC)
    // ====================================================================================
    UFUNCTION(Server, Reliable)
    void Server_ProcessSkill(FName SkillRowName, FVector TargetLocation);
    
    UFUNCTION(Server, Reliable)
    void Server_PlayMontage(UAnimMontage* MontageToPlay);
    
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayMontage(UAnimMontage* MontageToPlay);
    
    UFUNCTION(Server, Reliable)
    void Server_StopMontage(UAnimMontage* MontageToStop);
    
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_StopMontage(UAnimMontage* MontageToStop);

    // 범용 상호작용 (기둥 회전, 보트 이동 등)
    UFUNCTION(Server, Reliable)
    void Server_InteractActor(AActor* TargetActor, FVector Direction);
    
    // 기둥 파괴 명령
    UFUNCTION(Server, Reliable)
    void Server_TriggerPillarFall(APillar* TargetPillar);
    
    // 보트 탑승/하차 상태 동기화
    UFUNCTION(Server, Reliable)
    void Server_SetBoatRideState(AMagicBoat* Boat, bool bRiding);

protected:
    // ====================================================================================
    //  섹션 3: 라이프사이클 이벤트
    // ====================================================================================
    virtual void BeginPlay() override;
    virtual void OnDeath() override;
    
    // ====================================================================================
    //  섹션 4: 시점 조작 및 상호작용
    // ====================================================================================
    // 부모 클래스의 Look 함수 대체 (상호작용 시점 제어 포함)
    virtual void Look(const FInputActionValue& Value) override;
    
    // 크로스헤어가 가리키는 실제 월드 좌표 계산
    FVector GetCrosshairTargetLocation();

    // 상호작용 가능한 타겟 감지용 타이머
    FTimerHandle InteractionTimerHandle;
    void CheckInteractableTarget();

    // 염력 상호작용 최대 사거리
    UPROPERTY(EditAnywhere, Category = "Telekinesis")
    float TraceDistance = 5000.f;

    // ====================================================================================
    //  섹션 5: 전투 및 스킬 시스템
    // ====================================================================================
    virtual void Attack() override;
    virtual void Skill1() override;
    virtual void Skill2() override;
    
    // 스킬 데이터 및 쿨타임 관리
    TMap<FName, FSkillData*> SkillDataCache;
    
    UPROPERTY()
    TMap<FName, FTimerHandle> SkillTimers;

    void ProcessSkill(FName SkillRowName, FVector TargetLocation = FVector::ZeroVector);
    
    // 콤보 시스템
    int32 CurrentComboIndex = 0;
    FTimerHandle ComboResetTimer;
    void ResetCombo();

    // --- 스킬 1 (기본 투사체) ---
    UPROPERTY(EditDefaultsOnly, Category = "VFX")
    UParticleSystem* Skill1CastEffect;

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlaySkill1VFX();
    
    FTimerHandle ProjectileTimerHandle;
    void SpawnDelayedProjectile(UClass* ProjectileClass, float DamageAmount, FVector TargetLocation);

    // 투사체 발사 각도 보정 (위로 띄우는 정도)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    float ProjectilePitchOffset = 2.0f;

    // --- 스킬 2 (바위 소환) ---
    UPROPERTY(EditDefaultsOnly, Category = "VFX")
    UParticleSystem* Skill2CastEffect;
    
    UPROPERTY()
    UParticleSystemComponent* Skill2CastComp;

    FTimerHandle RockSpawnTimerHandle;
    void SpawnSkill2Rock(UClass* RockClass, float DamageAmount);
    
    // --- 무기 이펙트 (트레일 및 아우라) ---
    UPROPERTY()
    UStaticMeshComponent* CachedWeaponMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    TObjectPtr<UStaticMeshComponent> WeaponRootComp;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
    UNiagaraSystem* StaffTrailVFX;

    UPROPERTY()
    UNiagaraComponent* StaffTrailComp;
    
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_SetTrailActive(bool bActive);
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
    UNiagaraSystem* JobAuraVFX;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
    FVector JobAuraScale = FVector(1.0f);
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VFX")
    UNiagaraComponent* JobAuraComp;

    UFUNCTION(Server, Reliable)
    void Server_SetAuraActive(bool bActive);

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_SetAuraActive(bool bActive);

    // ====================================================================================
    //  섹션 6: 직업 능력 (염력 조종)
    // ====================================================================================
    virtual void JobAbility() override;
    void EndJobAbility();

    // 조종 상태 플래그
    bool bIsPillarMode = false;
    bool bIsBoatMode = false;

    // 기둥 관련 타겟
    UPROPERTY()
    APillar* FocusedPillar;
    UPROPERTY()
    APillar* CurrentTargetPillar;

    // 보트 및 범용 상호작용 타겟
    UPROPERTY()
    AActor* HoveredActor; 
    UPROPERTY()
    AActor* CurrentControlledActor; 

public:
    // ====================================================================================
    //  컴포넌트
    // ====================================================================================
    // 탑다운 시점 카메라 (보트 조종용)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    USpringArmComponent* TopDownSpringArm;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    UCameraComponent* TopDownCamera;
    
    // 카메라 줌인 타임라인 (기둥 조종용)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Timeline")
    UTimelineComponent* CameraTimelineComp;

    UPROPERTY(EditAnywhere, Category = "Timeline")
    UCurveFloat* CameraCurve;

private:
    // ====================================================================================
    //  내부 헬퍼 함수 및 변수
    // ====================================================================================
    UFUNCTION()
    void CameraTimelineProgress(float Alpha);

    UFUNCTION()
    void OnCameraTimelineFinished();

    void LockOnPillar(float DeltaTime);

    float DefaultArmLength;
    FVector DefaultSocketOffset;
};