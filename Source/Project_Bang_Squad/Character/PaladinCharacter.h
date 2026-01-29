#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "NiagaraSystem.h"
#include "PaladinCharacter.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API APaladinCharacter : public ABaseCharacter
{
    GENERATED_BODY()
    
public:
    // ====================================================================================
    //  섹션 1: 생성자 및 필수 오버라이드 (Constructor & Essentials)
    // ====================================================================================
    APaladinCharacter();
    

    // 리플리케이션 변수 등록 (네트워크)
    virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
    
    // 데미지 처리 함수 (서버 권한)
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
        class AController* EventInstigator, AActor* DamageCauser) override;

    // ====================================================================================
    //  섹션 7: 네트워크 및 애니메이션 유틸리티 (Network RPCs)
    //  (외부에서 호출될 가능성이 있어 public에 배치)
    // ====================================================================================
    UFUNCTION(Server, Reliable)
    void Server_PlayMontage(UAnimMontage* MontageToPlay);
    
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayMontage(UAnimMontage* MontageToPlay);
    
    UFUNCTION(NetMulticast, Reliable) 
    void Multicast_StopMontage(float BlendOutTime = 0.25f);
    void Multicast_StopMontage_Implementation(float BlendOutTime);

    //  특정 방향에서 오는 힘을 막고 있는지 검사 (바람, 투사체 등 공용)
    bool IsBlockingDirection(FVector IncomingDirection) const;

    // [ 외부(기믹)에서 방패 체력을 깎기 위한 함수
    void ConsumeShield(float Amount);

    FORCEINLINE class UStaticMeshComponent* GetShieldMesh() const { return ShieldMeshComp; }

protected:
    
    // ====================================================================================
    //  섹션 2: 초기화 및 상태 이벤트 (Initialization & State Events)
    // ====================================================================================
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void OnDeath() override;
    
    // 착지 시점 감지 (분쇄 스킬 VFX 처리용)
    virtual void Landed(const FHitResult& Hit) override;

    // ====================================================================================
    //  섹션 3: 기본 공격 및 콤보 (Basic Attack & Combo)
    // ====================================================================================
    virtual void Attack() override;

    // 콤보 시스템
    int32 CurrentComboIndex = 0;
    FTimerHandle ComboResetTimer;
    void ResetCombo();

    // 공격 판정 (Melee Trace)
    void StartMeleeTrace();
    void PerformMeleeTrace();
    void StopMeleeTrace();

    // 공격 판정 관련 변수
    FTimerHandle AttackHitTimerHandle;
    FTimerHandle HitLoopTimerHandle;
    FVector LastHammerLocation;
    TSet<AActor*> SwingDamagedActors;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    FVector HammerHitBoxSize = FVector(30.0f, 30.0f, 30.0f);                       

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    float HitDuration = 0.25f; 

    // [네트워크] 서버가 클라이언트에게 "타격감 연출해!"라고 명령하는 함수
    UFUNCTION(Client, Reliable)
    void Client_TriggerHitStop();
    void Client_TriggerHitStop_Implementation(); // 언리얼이 자동으로 생성하는 구현부
    
    // [역경직] 시간을 원래대로 복구하는 함수
    void RestoreTimeDilation();
    // [역경직] 시간 복구용 타이머 핸들
    FTimerHandle HitStopTimer;
    // [역경직] 멈추는 시간
    UPROPERTY(EditAnywhere, Category = "Combat|Feel")
    float HitStopDuration = 0.1f;
    // [역경직] 얼마나 느려질지
    UPROPERTY(EditAnywhere, Category = "Combat|Feel")
    float HitStopTimeDilation = 0.01f;
    // 에디터에서 카메라 흔들림 블루프린트를 넣을 변수
    UPROPERTY(EditAnywhere, Category = "Combat|Feel")
    TSubclassOf<class UCameraShakeBase> HitShakeClass;
    
    // ====================================================================================
    //  섹션 4: 스킬 시스템 (Skill System - Smash)
    // ====================================================================================
    virtual void Skill1() override; // 분쇄
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
    UNiagaraSystem* SmashImpactVFX;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
    FVector SmashVFXScale;
    
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlaySmashVFX(FVector Location);
    
    // 데이터 테이블 및 처리 로직
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data")
    UDataTable* SkillDataTable;
    
    TMap<FName, FSkillData*> SkillDataCache;
    
    void ProcessSkill(FName SkillRowName);

    UFUNCTION(Server, Reliable)
    void Server_ProcessSkill(FName SkillRowName);

    // 스킬 딜레이 및 데미지 변수
    float CurrentActionDelay = 0.0f;
    float CurrentSkillDamage = 0.0f;
    bool bHasDealtSmashDamage = false;

    // 타이머가 끝나면 실제로 폭발 데미지를 주는 함수 (인자 추가됨)
    UFUNCTION()
    void PerformSmashDamage(float SmashingDamage);

    // ====================================================================================
    //  섹션 5: 방어 시스템 (Job Ability - Shield)
    // ====================================================================================
    virtual void JobAbility() override;
    void EndJobAbility();

    // 방패 활성화 로직 (타이머 -> ActivateGuard -> Server_SetGuard)
    FTimerHandle ShieldActivationTimer;
    void ActivateGuard();

    UFUNCTION(Server, Reliable)
    void Server_SetGuard(bool bNewGuarding);

    UFUNCTION()
    void OnRep_IsGuarding();

    // 방패 상태 관리
    UPROPERTY(ReplicatedUsing = OnRep_IsGuarding, BlueprintReadOnly, Category = "Combat")
    bool bIsGuarding;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Shield")
    bool bIsShieldBroken;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Shield")
    float CurrentShieldHP;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield")
    float MaxShieldHP = 200.0f;

    // 방패 회복 및 파괴
    void RegenShield();
    void OnShieldBroken();
    void RestoreShieldAfterCooldown();
    void SetShieldActive(bool bActive);

    FTimerHandle ShieldRegenTimer;
    FTimerHandle ShieldBrokenTimerHandle;

    UPROPERTY(EditAnywhere, Category = "Shield")
    float ShieldRegenRate = 5.0f;
    
    UPROPERTY(EditAnywhere, Category = "Shield")
    float ShieldRegenDelay = 2.0f;

    float GuardWalkSpeed = 250.0f;

    // 방패 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    class UStaticMeshComponent* ShieldMeshComp;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
    class UWidgetComponent* ShieldBarWidgetComp;

private:
    // ====================================================================================
    //  Internal Helpers & State (Private)
    // ====================================================================================
    
    // 무기 컴포넌트 캐싱 (최적화)
    UPROPERTY()
    UStaticMeshComponent* CachedWeaponMesh;
    
    // 분쇄 점프 상태 확인용 플래그
    bool bIsSmashing = false;
    
    // 이번 스킬 데미지 임시 저장 (CPP 로직에서 사용)
    float CurrentSmashDamage = 0.0f;
    
    // 스킬 쿨타임 관리 맵
    UPROPERTY()
    TMap<FName, FTimerHandle> SkillTimers;
    
    
};