#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "PaladinCharacter.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API APaladinCharacter : public ABaseCharacter
{
    GENERATED_BODY()
    
public:
    APaladinCharacter();
    
    // =============================================================
    // [네트워크 RPC]
    // =============================================================
    UFUNCTION(Server, Reliable)
    void Server_PlayMontage(UAnimMontage* MontageToPlay);
    
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayMontage(UAnimMontage* MontageToPlay);
    
    UFUNCTION(NetMulticast, Reliable) 
    void Multicast_StopMontage(float BlendOutTime = 0.25f);
    void Multicast_StopMontage_Implementation(float BlendOutTime);
    
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
        class AController* EventInstigator, AActor* DamageCauser) override;
    
    virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void OnDeath() override;
    virtual void Attack() override;
    // 스킬 1 (분쇄)
    virtual void Skill1() override;
    // 착지 시점 감지 (찍기 데미지 용도)
    virtual void Landed(const FHitResult& Hit) override;
    
    // 스킬의 딜레이 시간 저장용
    float CurrentActionDelay = 0.0f;
    
    // 타이머가 끝나면 실제로 폭발 데미지를 주는 함수
    UFUNCTION()
    void PerformSmashDamage(float SmashingDamage);
    
    // =============================================================
    // [공격 판정 관련 변수 및 함수]
    // =============================================================
    float CurrentSkillDamage = 0.0f;
    bool bHasDealtSmashDamage = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    FVector HammerHitBoxSize = FVector(30.0f, 30.0f, 30.0f);                       
    
    FTimerHandle AttackHitTimerHandle;
    FTimerHandle HitLoopTimerHandle;
    FTimerHandle ShieldBrokenTimerHandle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    float HitDuration = 0.25f; 

    FVector LastHammerLocation;
    TSet<AActor*> SwingDamagedActors;

    void StartMeleeTrace();
    void PerformMeleeTrace();
    void StopMeleeTrace();

    // =============================================================
    // [콤보 및 데이터 테이블]
    // =============================================================
    int32 CurrentComboIndex = 0;
    FTimerHandle ComboResetTimer;
    void ResetCombo();
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data")
    UDataTable* SkillDataTable;
    void ProcessSkill(FName SkillRowName);
    
    // =============================================================
    // [직업 능력 (방패)] 
    // =============================================================
    virtual void JobAbility() override;
    void EndJobAbility();
    // 쿨타임 끝나면 방패를 풀피로 복구하는 함수
    void RestoreShieldAfterCooldown();
    
    
    // =============================================================
    // [방패 시스템]
    // =============================================================
    
    // 방패 메쉬
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    class UStaticMeshComponent* ShieldMeshComp;
    
    // 방패 체력바 위젯 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
    class UWidgetComponent* ShieldBarWidgetComp;
    
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield")
    float MaxShieldHP = 200.0f;
    
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Shield")
    float CurrentShieldHP;
    
    UPROPERTY(EditAnywhere, Category = "Shield")
    float ShieldRegenRate = 5.0f;
    
    UPROPERTY(EditAnywhere, Category = "Shield")
    float ShieldRegenDelay = 2.0f;
    
    // 상태 관리
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Shield")
    bool bIsShieldBroken;
    
    FTimerHandle ShieldRegenTimer;
    
    // 방패 생성 지연 타이머
    FTimerHandle ShieldActivationTimer;
    
    // 타이머 끝나면 실제로 서버에 요청하는 함수
    void ActivateGuard();
    // 가드 상태 동기화 
    UPROPERTY(ReplicatedUsing = OnRep_IsGuarding, BlueprintReadOnly, Category = "Combat")
    bool bIsGuarding;
    
    UFUNCTION()
    void OnRep_IsGuarding();
    
    UFUNCTION(Server, Reliable)
    void Server_SetGuard(bool bNewGuarding);
    
    UFUNCTION(Server, Reliable)
    void Server_ProcessSkill(FName SkillRowName);
    

    
    void RegenShield();
    void OnShieldBroken();
    void SetShieldActive(bool bActive);
    
    float GuardWalkSpeed = 250.0f;
    
private:
    // 무기 컴포넌트를 미리 저장해둘 변수
    UPROPERTY()
    UStaticMeshComponent* CachedWeaponMesh;
    
    // 지금 분쇄 스킬로 점프 중인가? (일반 점프와 구분용)
    bool bIsSmashing = false;
    
    // 이번 스킬의 데미지 임시 저장 (데이터 테이블에서 가져온 값)
    float CurrentSmashDamage = 0.0f;
    
    // 쿨타임 관리
    UPROPERTY()
    TMap<FName, FTimerHandle> SkillTimers;
    
    // 바닥 파이는 이펙트 (나중에 채워넣을 변수)
    UPROPERTY(EditDefaultsOnly, Category = "VFX")
    UParticleSystem* SmashImpactVFX;
};