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
    // [Section 1] 초기화 및 코어 오버라이드 (Initialization & Core Overrides)
    // ====================================================================================
    AMageCharacter();

    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

    // ====================================================================================
    // [Section 2] 컴포넌트 (Components)
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

    // ====================================================================================
    // [Section 3] 네트워크 RPC (Network RPCs)
    // ====================================================================================
    UFUNCTION(Server, Reliable) void Server_ProcessSkill(FName SkillRowName, FVector TargetLocation = FVector::ZeroVector);
    

    // 범용 상호작용 및 기믹 제어
    UFUNCTION(Server, Reliable) void Server_InteractActor(AActor* TargetActor, FVector Direction);
    UFUNCTION(Server, Reliable) void Server_TriggerPillarFall(APillar* TargetPillar);
    UFUNCTION(Server, Reliable) void Server_SetBoatRideState(AMagicBoat* Boat, bool bRiding);
    
    // 현재 조종 상태를 서버에 보고
    UFUNCTION(Server, Reliable) void Server_SyncJobState(bool bPillar, bool bBoat);
    
    // 서버가 클라이언트에게 강제로 스킬을 끄라고 명령 (피격 시 카메라 원상복구됨)
    UFUNCTION(Client, Reliable) void Client_ForceEndJobAbility();
    
    // ====================================================================================
    // [Section 4] 라이프사이클 (Lifecycle)
    // ====================================================================================
    virtual void BeginPlay() override;
    virtual void OnDeath() override;
    
    // ====================================================================================
    // [Section 5] 조작 및 상호작용 (Input & Interaction)
    // ====================================================================================
    // 시점 제어 및 기믹 조작 (부모 클래스 함수 오버라이드)
    virtual void Look(const FInputActionValue& Value) override;
    
    // 크로스헤어(십자선)가 가리키는 실제 월드 좌표 계산
    FVector GetCrosshairTargetLocation();

    // 상호작용 가능한 타겟 감지
    FTimerHandle InteractionTimerHandle;
    void CheckInteractableTarget();

    // 염력 상호작용 최대 사거리
    UPROPERTY(EditAnywhere, Category = "Telekinesis")
    float TraceDistance = 5000.f;

    // ====================================================================================
    // [Section 6] 전투 시스템 (Combat System)
    // ====================================================================================
    virtual void Attack() override;
    virtual void Skill1() override;
    virtual void Skill2() override;
    
    // 스킬 데이터 캐싱 및 쿨타임 관리
    TMap<FName, FSkillData*> SkillDataCache;
    UPROPERTY() TMap<FName, FTimerHandle> SkillTimers;
    void ProcessSkill(FName SkillRowName, FVector TargetLocation = FVector::ZeroVector);
    
    // 콤보 공격 처리
    int32 CurrentComboIndex = 0;
    FTimerHandle ComboResetTimer;
    void ResetCombo();

    // --- 에임 및 투사체 설정 (Aiming & Projectiles) ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Aiming") float ProjectileForwardOffset = 50.0f;  // 스폰 전방 오프셋
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Aiming") float ProjectileDownwardOffset = 60.0f; // 스폰 하단 오프셋
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Aiming") float AimTraceDistance = 5000.0f;       // 조준 최대 거리
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat") float ProjectilePitchOffset = 2.0f;            // 발사 각도 보정

    // --- 스킬 로직 ---
    FTimerHandle ProjectileTimerHandle;
    void SpawnDelayedProjectile(UClass* ProjectileClass, float DamageAmount, FVector TargetLocation);

    FTimerHandle RockSpawnTimerHandle;
    void SpawnSkill2Rock(UClass* RockClass, float DamageAmount);

    // --- VFX (시각 효과) ---
    UPROPERTY(EditDefaultsOnly, Category = "VFX") UParticleSystem* Skill1CastEffect;
    UPROPERTY(EditDefaultsOnly, Category = "VFX") UParticleSystem* Skill2CastEffect;
    UPROPERTY() UParticleSystemComponent* Skill2CastComp;

    UFUNCTION(NetMulticast, Unreliable) void Multicast_PlaySkill1VFX();
    UFUNCTION(NetMulticast, Unreliable) void Multicast_PlaySkill2VFX();
    UFUNCTION(NetMulticast, Unreliable) void Multicast_StopSkill2VFX();

    // --- 무기 및 아우라 ---
    UPROPERTY() UStaticMeshComponent* CachedWeaponMesh;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    TObjectPtr<UStaticMeshComponent> WeaponRootComp;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX") UNiagaraSystem* StaffTrailVFX;
    UPROPERTY() UNiagaraComponent* StaffTrailComp;
    UFUNCTION(NetMulticast, Unreliable) void Multicast_SetTrailActive(bool bActive);
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX") UNiagaraSystem* JobAuraVFX;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX") FVector JobAuraScale = FVector(1.0f);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VFX") UNiagaraComponent* JobAuraComp;

    UFUNCTION(Server, Reliable) void Server_SetAuraActive(bool bActive);
    UFUNCTION(NetMulticast, Reliable) void Multicast_SetAuraActive(bool bActive);

    // ====================================================================================
    // [Section 7] 직업 능력 - 염력 (Job Ability - Telekinesis)
    // ====================================================================================
    virtual void JobAbility() override;
    void EndJobAbility();

    // 상태 플래그
    bool bIsPillarMode = false;
    bool bIsBoatMode = false;

    // 상호작용 타겟 캐싱
    UPROPERTY() APillar* FocusedPillar;
    UPROPERTY() APillar* CurrentTargetPillar;
    UPROPERTY() AActor* HoveredActor; 
    UPROPERTY() AActor* CurrentControlledActor; 

private:
    // ====================================================================================
    // 내부 헬퍼 함수 (Internal Helpers)
    // ====================================================================================
    UFUNCTION() void CameraTimelineProgress(float Alpha);
    UFUNCTION() void OnCameraTimelineFinished();

    void LockOnPillar(float DeltaTime);

    float DefaultArmLength;
    FVector DefaultSocketOffset;
};