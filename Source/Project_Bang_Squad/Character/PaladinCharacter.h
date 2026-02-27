#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "PaladinCharacter.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API APaladinCharacter : public ABaseCharacter
{
    GENERATED_BODY()

public:
    // ====================================================================================
    //  [Section 1] 생성자 및 필수 오버라이드 (Constructor & Essentials)
    // ====================================================================================
    APaladinCharacter();

    virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

    // ====================================================================================
    //  [Section 2] 방어 및 기믹 처리 (Defense & Interaction)
    // ====================================================================================
    // 특정 방향에서 오는 공격 방어 여부 확인 (내적 계산)
    bool IsBlockingDirection(FVector IncomingDirection) const;

    // 외부(기믹 등)에서 방패 체력 소모 시 호출
    void ConsumeShield(float Amount);

    // 방패 메쉬 접근자
    FORCEINLINE class UStaticMeshComponent* GetShieldMesh() const { return ShieldMeshComp; }
    

    // ====================================================================================
    //  [Section 4] 방패 상태 변수 (Replicated Properties)
    // ====================================================================================
    UPROPERTY(ReplicatedUsing = OnRep_IsGuarding, BlueprintReadOnly, Category = "Combat|Shield")
    bool bIsGuarding;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Combat|Shield")
    bool bIsShieldBroken;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Combat|Shield")
    float CurrentShieldHP;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Shield")
    float MaxShieldHP = 200.0f;

protected:
    // ====================================================================================
    //  [Section 5] 라이프사이클 및 이벤트 (Lifecycle & Events)
    // ====================================================================================
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void OnDeath() override;
    virtual void Landed(const FHitResult& Hit) override; // 분쇄 스킬 착지 감지용

    // ====================================================================================
    //  [Section 6] 기본 공격 및 콤보 (Basic Attack & Combo)
    // ====================================================================================
    virtual void Attack() override;

    // 콤보 시스템
    int32 CurrentComboIndex = 0;
    FTimerHandle ComboResetTimer;
    void ResetCombo();

    // 근접 공격 판정 (Trace)
    void StartMeleeTrace();
    void PerformMeleeTrace();
    void StopMeleeTrace();

    // 판정 관련 변수
    FTimerHandle AttackHitTimerHandle;
    FTimerHandle HitLoopTimerHandle;
    FVector LastHammerLocation;
    
    UPROPERTY()
    TSet<AActor*> SwingDamagedActors;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Trace")
    FVector HammerHitBoxSize = FVector(30.0f, 30.0f, 30.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Trace")
    float HitDuration = 0.25f;

    // ====================================================================================
    //  [Section 7] 타격감 및 VFX (Feel & Visual Effects)
    // ====================================================================================
    // [역경직] 클라이언트 화면 멈춤 및 카메라 흔들림
    UFUNCTION(Client, Reliable)
    void Client_TriggerHitStop();
    
    void RestoreTimeDilation();
    FTimerHandle HitStopTimer;

    UPROPERTY(EditAnywhere, Category = "Combat|Feel")
    float HitStopDuration = 0.1f;

    UPROPERTY(EditAnywhere, Category = "Combat|Feel")
    float HitStopTimeDilation = 0.01f;

    UPROPERTY(EditAnywhere, Category = "Combat|Feel")
    TSubclassOf<class UCameraShakeBase> HitShakeClass;

    // 무기 트레일 이펙트
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
    UNiagaraSystem* WeaponTrailVFX;

    UPROPERTY()
    UNiagaraComponent* WeaponTrailComp;

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_SetTrailActive(bool bActive);

    // ====================================================================================
    //  [Section 8] 스킬 시스템 (Skill System)
    // ====================================================================================
    // 스킬 데이터 캐싱
    TMap<FName, FSkillData*> SkillDataCache;
    void ProcessSkill(FName SkillRowName);

    UFUNCTION(Server, Reliable)
    void Server_ProcessSkill(FName SkillRowName);

    // --- Skill 1: 분쇄 (Smash) ---
    virtual void Skill1() override;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX|Smash")
    UNiagaraSystem* SmashImpactVFX;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX|Smash")
    FVector SmashVFXScale;

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlaySmashVFX(FVector Location);

    UFUNCTION()
    void PerformSmashDamage(float SmashingDamage); // 타이머 호출용

    // --- Skill 2: 망치 소환 (Hammer) ---
    virtual void Skill2() override;

    UFUNCTION(Server, Reliable)
    void Server_Skill2();

    UPROPERTY(EditAnywhere, Category = "VFX|Skill2")
    UNiagaraSystem* Skill2CastVFX;

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlaySkill2CastVFX();

    UFUNCTION()
    void SpawnSkill2Hammer(float FinalDamage);

    UPROPERTY(EditAnywhere, Category = "Combat|Skill")
    TSubclassOf<class APaladinSkill2Hammer> Skill2HammerClass;

    float Skill2ReadyTime = 0.0f; // 로컬 쿨타임 체크용

    // ====================================================================================
    //  [Section 9] 직업 능력 - 방패 (Job Ability)
    // ====================================================================================
    virtual void JobAbility() override;
    void EndJobAbility();

    // 방패 활성화 로직
    FTimerHandle ShieldActivationTimer;
    void ActivateGuard();

    UFUNCTION(Server, Reliable)
    void Server_SetGuard(bool bNewGuarding);

    UFUNCTION()
    void OnRep_IsGuarding();

    // 방패 상태 관리
    void RegenShield();
    void OnShieldBroken();
    void RestoreShieldAfterCooldown();
    void SetShieldActive(bool bActive);

    FTimerHandle ShieldRegenTimer;
    FTimerHandle ShieldBrokenTimerHandle;

    UPROPERTY(EditAnywhere, Category = "Combat|Shield")
    float ShieldRegenRate = 5.0f;

    UPROPERTY(EditAnywhere, Category = "Combat|Shield")
    float ShieldRegenDelay = 2.0f;

    // 방패 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UStaticMeshComponent* ShieldMeshComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UWidgetComponent* ShieldBarWidgetComp;

private:
    // ====================================================================================
    //  [Internal] 내부 변수 (Private Variables)
    // ====================================================================================
    // 무기 컴포넌트 캐싱 (최적화)
    UPROPERTY()
    UStaticMeshComponent* CachedWeaponMesh;

    // 상태 플래그
    bool bIsSmashing = false;           // 분쇄 점프 중인지
    bool bHasDealtSmashDamage = false;  // 분쇄 데미지 중복 방지

    // 스킬 관련 임시 변수
    float CurrentSmashDamage = 0.0f;
    float CurrentActionDelay = 0.0f;
    float CurrentSkillDamage = 0.0f;

    // 쿨타임 관리 맵
    UPROPERTY()
    TMap<FName, FTimerHandle> SkillTimers;
};