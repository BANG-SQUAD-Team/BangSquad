#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TimerManager.h" 

#include "EnemyCharacterBase.generated.h"

class UAnimMontage;
class UHealthComponent;

UCLASS()
class PROJECT_BANG_SQUAD_API AEnemyCharacterBase : public ACharacter
{
    GENERATED_BODY()

public:
    AEnemyCharacterBase();

protected:
    virtual void BeginPlay() override;

public:
    // 데미지 받는 함수 (Override)
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, 
        class AController* EventInstigator, AActor* DamageCauser) override;

    // ===== Hit React =====
    UFUNCTION(BlueprintCallable, Category = "HitReact")
    void ReceiveHitReact();

    UFUNCTION(BlueprintPure, Category = "HitReact")
    bool IsHitReacting() const { return bIsHitReacting; }

    // ===== Death =====
    UFUNCTION(BlueprintCallable, Category = "Death")
    void ReceiveDeath();

    UFUNCTION(BlueprintPure, Category = "Death")
    bool IsDead() const { return bIsDead; }

protected:
    // 사망 시작 시 자식 클래스(EnemyNormal)에서 로직 처리용
    virtual void OnDeathStarted();
    // 자식 클래스에서 오버라이드할 수 있는 체력 변경 알림 함수
    virtual void OnHPChanged(float CurrentHP, float MaxHP);

    // ===== HealthComponent Auto Bind =====
    UFUNCTION()
    void HandleDeadFromHealth();

    UFUNCTION()
    void HandleHealthChangedFromHealth(float NewHealth, float InMaxHealth);

    // ===== Hit React Settings =====
    UPROPERTY(EditDefaultsOnly, Category = "HitReact")
    TObjectPtr<UAnimMontage> HitReactMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "HitReact", meta = (ClampMin = "0.05", ClampMax = "1.0"))
    float HitReactSpeedMultiplier = 0.35f;

    UPROPERTY(EditDefaultsOnly, Category = "HitReact", meta = (ClampMin = "0.05", ClampMax = "5.0"))
    float HitReactMinDuration = 0.25f;

    UPROPERTY(EditDefaultsOnly, Category = "HitReact")
    bool bIgnoreHitReactWhileActive = true;

    // ===== Death Settings =====
    UPROPERTY(EditDefaultsOnly, Category = "Death")
    TObjectPtr<UAnimMontage> DeathMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Death")
    bool bLoopDeathMontage = false;

    UPROPERTY(EditDefaultsOnly, Category = "Death", meta = (EditCondition = "bLoopDeathMontage"))
    FName DeathLoopSectionName = TEXT("Loop");

    UPROPERTY(EditDefaultsOnly, Category = "Death")
    bool bEnableRagdollOnDeath = true;

    UPROPERTY(EditDefaultsOnly, Category = "Death", meta = (ClampMin = "0.0", ClampMax = "10.0", EditCondition = "bEnableRagdollOnDeath"))
    float DeathToRagdollDelay = 0.25f;

    UPROPERTY(EditDefaultsOnly, Category = "Death")
    bool bDestroyAfterDeath = true;

    UPROPERTY(EditDefaultsOnly, Category = "Death", meta = (ClampMin = "0.0", ClampMax = "60.0", EditCondition = "bDestroyAfterDeath"))
    float DestroyDelay = 8.0f;

protected:
    void StartHitReact(float Duration);
    void EndHitReact();

    void StartDeath();
    void EnterRagdoll();

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayHitReactMontage();
    void Multicast_PlayHitReactMontage_Implementation();

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayDeathMontage();
    void Multicast_PlayDeathMontage_Implementation();

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_EnterRagdoll();
    void Multicast_EnterRagdoll_Implementation();

private:
    bool bIsHitReacting = false;
    bool bIsDead = false;

    float DefaultMaxWalkSpeed = 0.f;

    FTimerHandle HitReactTimer;
    FTimerHandle DeathToRagdollTimer;

protected:
    uint32 MyUniqueID = 0;

    void GenerateUniqueID();

protected:
    // 어떤 투사체를 쏠지 (블루프린트에서 할당)
    UPROPERTY(EditAnywhere, Category = "Combat|Slash")
    TSubclassOf<class ASlashProjectile> SlashClass;

    // [중요] 발사 높이 오프셋 (0이면 몸통 정중앙, +면 머리쪽, -면 다리쪽)
    UPROPERTY(EditAnywhere, Category = "Combat|Slash")
    float SlashZOffset = 0.0f;

    // [중요] 몸에서 얼마나 앞에서 생성할지 (기존 150.f 유지용)
    UPROPERTY(EditAnywhere, Category = "Combat|Slash")
    float SlashForwardOffset = 150.0f;

    /** [찾으시는 것!] 참격 데미지 수치 */
    UPROPERTY(EditAnywhere, Category = "Combat|Slash")
    float SlashDamage = 30.0f; // <-- 이게 디테일 창에 다시 나타납니다.

    // =================================================================
    // [UI] 몬스터 머리 위 체력바 위젯
    // =================================================================
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
    class UWidgetComponent* HealthWidgetComp;

    // 체력바 위젯을 찾아서 갱신하는 헬퍼 함수
    void UpdateHealthBar(float CurrentHP, float MaxHP);

    // 모든 클라이언트에게 체력바 UI 갱신을 지시하는 멀티캐스트 함수
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_UpdateHealthBar(float CurrentHP, float InMaxHP);
public:
    // 노티파이에서 호출할 공통 함수
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void AnimNotify_SpawnSlash();
    
    // =================================================================
    // 데미지 플로팅 관련 시스템
    // =================================================================
    // 블루프린트에서 데미지 텍스트 액터를 할당받을 변수
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<class ADamageTextActor> DamageTextClass;

    // 모든 클라이언트 화면에 데미지 텍스트를 띄우라고 명령
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_SpawnDamageText(float Damage, FVector Location);
};