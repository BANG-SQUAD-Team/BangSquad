// Source/Project_Bang_Squad/Character/StageBoss/Stage2Boss.h

#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBossData.h"
#include "Stage2Boss.generated.h"

// ���� ����
class AEnemySpawner;
class UHealthComponent;
class UAnimMontage;
class AQTE_Trap;

UCLASS()
class PROJECT_BANG_SQUAD_API AStage2Boss : public AEnemyCharacterBase
{
    GENERATED_BODY()

public:
    AStage2Boss();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

    // --- [Actions] ---
    float PlayMeleeAttackAnim();
    void PerformWebShot(AActor* Target);
    // [추가] 실제 발사 (Notify가 호출함)
    UFUNCTION(BlueprintCallable)
    void FireWebProjectile();
    void PerformSmashAttack(AActor* Target);

    // --- [Phase & QTE] ---
    bool IsInvinciblePhase() const { return bIsInvincible; }
    void CheckHealthPhase();
    void StartQTEPattern(AActor* Target);

    // [추가] 내려찍기 피격 판정 (몽타주 Notify에서 호출)
    UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
    void PerformSmashHitCheck();
    // --- [Settings] ---
    UPROPERTY(EditAnywhere, Category = "Boss|Combat")
    TSubclassOf<class AQTE_Trap> QTETrapClass;

    // [추가] 내려찍기 판정 범위 (반지름)
    UPROPERTY(EditAnywhere, Category = "Boss|Combat")
    float SmashRadius = 300.0f;

    // [추가] 보스 앞쪽으로 얼마나 떨어져서 생성할지 (X축 오프셋)
    UPROPERTY(EditAnywhere, Category = "Boss|Combat")
    float SmashForwardOffset = 300.0f;

    // [추가] 위아래 높이 조절 (Z축 오프셋 - 땅에 묻히면 올리거나 내리기 위함)
    UPROPERTY(EditAnywhere, Category = "Boss|Combat")
    float SmashZOffset = -50.0f;

    // 애니메이션 노티파이에서 호출할 실제 넉백 함수
    UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
    void ExecuteFollowUpKnockback();

    // 블루프린트에서 설정할 추가타 몽타주
    UPROPERTY(EditAnywhere, Category = "Boss|Anim")
    UAnimMontage* QTEFollowUpMontage;


    // [Getter]
    UEnemyBossData* GetBossData() const { return BossData; }

    // [QTE ��� ó��]
    // Ŭ���̾�Ʈ�� RPC�� ���� PlayerController�� �� �Լ��� ȣ���մϴ�.
    UFUNCTION(BlueprintCallable)
    void OnQTEResult(bool bSuccess);
    
    UFUNCTION(Server, Reliable)
    void ServerRPC_QTEResult(class APlayerController* Player, bool bSuccess);

protected:
    void CheckMinionsStatus();

protected:
    // --- [Components] ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Data")
    TObjectPtr<UEnemyBossData> BossData;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UHealthComponent> HealthComponent;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Phase")
    TObjectPtr<AEnemySpawner> MinionSpawner;

    // --- [Settings] ---
    UPROPERTY(EditAnywhere, Category = "Boss|Combat")
    TSubclassOf<AActor> WebProjectileClass;

    UPROPERTY(EditAnywhere, Category = "Boss|Combat")
    float WebProjectileSpeed = 1500.0f;

    UPROPERTY(EditAnywhere, Category = "Boss|Combat")
    TSubclassOf<AActor> SmashEffectClass;

    // --- [State] ---
    bool bIsInvincible = false;
    bool bPhase70Triggered = false;
    bool bPhase30Triggered = false;

    // --- [Anim] ---
    UPROPERTY(EditAnywhere, Category = "Boss|Anim")
    UAnimMontage* WebShotMontage;

    UPROPERTY(EditAnywhere, Category = "Boss|Anim")
    UAnimMontage* SmashMontage;

   

    // [추가] 넉백 힘 (기본값 설정)
    UPROPERTY(EditAnywhere, Category = "Boss|Combat")
    float FollowUpKnockbackPower = 2000.0f;

    // [추가] 추가타 판정 거리 (앞으로 얼마나 멀리)
    UPROPERTY(EditAnywhere, Category = "Boss|Combat")
    float FollowUpTraceForwardOffset = 400.0f;

    // [추가] 추가타 판정 높이 (Z축 조절 - 땅에 묻히거나 허공을 칠 때 사용)
    UPROPERTY(EditAnywhere, Category = "Boss|Combat")
    float FollowUpTraceZOffset = 50.0f;

    // [추가] 추가타 판정 두께 (구체의 반지름)
    UPROPERTY(EditAnywhere, Category = "Boss|Combat")
    float FollowUpTraceRadius = 300.0f;

};