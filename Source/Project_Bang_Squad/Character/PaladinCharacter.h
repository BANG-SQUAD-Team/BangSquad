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
    
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
        class AController* EventInstigator, AActor* DamageCauser) override;
    
    virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    
    virtual void Attack() override;

    // =============================================================
    // [공격 판정 관련 변수 및 함수]
    // =============================================================
    float CurrentSkillDamage = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    FVector HammerHitBoxSize = FVector(30.0f, 30.0f, 30.0f);                       
    
    FTimerHandle AttackHitTimerHandle;
    FTimerHandle HitLoopTimerHandle;

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
    
    // =============================================================
    // [방패 시스템]
    // =============================================================
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    class UStaticMeshComponent* ShieldMeshComp;
    
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
    
    void RegenShield();
    void OnShieldBroken();
    void SetShieldActive(bool bActive);
    
    float GuardWalkSpeed = 250.0f;
};