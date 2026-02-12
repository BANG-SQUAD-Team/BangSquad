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
    //  섹션 1: 생성자 및 필수 오버라이드 (Constructor & Essentials)
    // ====================================================================================
    AMageCharacter();

    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    
    // ====================================================================================
    //  섹션 7: 네트워크 및 유틸리티 (Network RPCs)
    // ====================================================================================
    UFUNCTION(Server, Reliable)
    void Server_ProcessSkill(FName SkillRowName, FVector TargetLocation);
    
    UFUNCTION(Server, Reliable)
    void Server_PlayMontage(UAnimMontage* MontageToPlay);
    
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayMontage(UAnimMontage* MontageToPlay);
    
    UFUNCTION(Server,Reliable)
    void Server_StopMontage(UAnimMontage* MontageToStop);
    
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_StopMontage(UAnimMontage* MontageToStop);

    // 범용 상호작용 RPC (기둥 파괴, 보트 이동 모두 처리)
    UFUNCTION(Server, Reliable)
    void Server_InteractActor(AActor* TargetActor, FVector Direction);
    
    // 기존 기둥 파괴 RPC
    UFUNCTION(Server, Reliable)
    void Server_TriggerPillarFall(APillar* TargetPillar);
    
    // 서버한테 보트 탑승/하차 상태를 알리는 함수
    UFUNCTION(Server, Reliable)
    void Server_SetBoatRideState(AMagicBoat* Boat, bool bRiding);

protected:
    
    FVector GetCrosshairTargetLocation();
    // ====================================================================================
    //  섹션 2: 초기화 및 상태 이벤트 (Initialization & State Events)
    // ====================================================================================
    virtual void BeginPlay() override;
    virtual void OnDeath() override;
    
    // ====================================================================================
    //  섹션 3: 조작 및 인터랙션 (Control & Interaction)
    // ====================================================================================
    
    // Look 함수 (override 제거하고 독자 함수로 선언 -> BaseCharacter::Look 대체)
    virtual void Look(const FInputActionValue& Value) override;
    
    // 범용 타겟 감지 함수 (타이머로 작동)
    void CheckInteractableTarget();
    FTimerHandle InteractionTimerHandle;

    UPROPERTY(EditAnywhere, Category = "Telekinesis")
    float TraceDistance = 5000.f;

    // ====================================================================================
    //  섹션 4: 전투 및 스킬 시스템 (Combat & Skills)
    // ====================================================================================
    virtual void Attack() override;
    virtual void Skill1() override;
    virtual void Skill2() override;
    
    // 스킬 1번 사용 시 재생할 파티클 (에디터에서 할당)
    UPROPERTY(EditDefaultsOnly, Category = "VFX")
    UParticleSystem* Skill1CastEffect;

    //  모든 클라이언트에서 파티클을 재생하라고 명령하는 함수
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlaySkill1VFX();
    
    // 에디터에서 넣을 지팡이 트레일 VFX
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
    UNiagaraSystem* StaffTrailVFX;

    // 실제 생성될 컴포넌트
    UPROPERTY()
    UNiagaraComponent* StaffTrailComp;
    
    // 무기 메쉬 캐싱 (지팡이에 붙이기 위함)
    UPROPERTY()
    UStaticMeshComponent* CachedWeaponMesh;

    // 멀티캐스트로 켜고 끄기
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_SetTrailActive(bool bActive);
    
    // 에디터에서 할당할 아우라 이펙트 (Loop 설정 필수!)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
    UNiagaraSystem* JobAuraVFX;

    //  아우라 크기 조절 변수 (기본값 1.0)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
    FVector JobAuraScale = FVector(1.0f);
    
    // 실제 생성될 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VFX")
    UNiagaraComponent* JobAuraComp;

    // 서버에게 아우라 상태 변경 요청
    UFUNCTION(Server, Reliable)
    void Server_SetAuraActive(bool bActive);

    // 모든 클라이언트에게 아우라 켜기/끄기 전파
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_SetAuraActive(bool bActive);
    
    // 콤보 시스템
    int32 CurrentComboIndex = 0;
    FTimerHandle ComboResetTimer;
    void ResetCombo();

    // 스킬 처리 및 투사체
    void ProcessSkill(FName SkillRowName, FVector TargetLocation = FVector::ZeroVector);
    
    FTimerHandle ProjectileTimerHandle;
    void SpawnDelayedProjectile(UClass* ProjectileClass, float DamageAmount, FVector TargetLocation);
    
    // 스킬 쿨타임 관리 (Key: 스킬이름, Value: 타이머 핸들)
    UPROPERTY()
    TMap<FName, FTimerHandle> SkillTimers;
    
    
    // 데이터 테이블 캐싱 변수
    TMap<FName, FSkillData*> SkillDataCache;

    // 스킬 2번 소환 타이머 핸들
    FTimerHandle RockSpawnTimerHandle;
    // 바위 소환 함수
    void SpawnSkill2Rock(UClass* RockClass, float DamageAmount);
    // 스킬 2번 이펙트
    UPROPERTY(EditDefaultsOnly, Category = "VFX")
    UParticleSystem* Skill2CastEffect;
    
    // 생성된 파티클을 제어하기 위해 저장할 변수
    UPROPERTY()
    UParticleSystemComponent* Skill2CastComp;
    
    // ====================================================================================
    //  섹션 5: 직업 능력 (Job Ability - Telekinesis/Boat)
    // ====================================================================================
    virtual void JobAbility() override;
    void EndJobAbility();

    // 상태 플래그
    bool bIsPillarMode = false;
    bool bIsBoatMode = false;

    // 기둥 관련 변수
    UPROPERTY()
    APillar* FocusedPillar;
    UPROPERTY()
    APillar* CurrentTargetPillar;

    // 보트(범용) 관련 변수
    UPROPERTY()
    AActor* HoveredActor; // 마우스 오버된 대상
    UPROPERTY()
    AActor* CurrentControlledActor; // 조종 중인 대상

public:
    // ====================================================================================
    //  컴포넌트 (Components)
    // ====================================================================================
    
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

private:
    // ====================================================================================
    //  Internal Helpers (Private)
    // ====================================================================================
    UFUNCTION()
    void CameraTimelineProgress(float Alpha);

    UFUNCTION()
    void OnCameraTimelineFinished();

    // 기둥 전용 락온 로직
    void LockOnPillar(float DeltaTime);

    float DefaultArmLength;
    FVector DefaultSocketOffset;
};