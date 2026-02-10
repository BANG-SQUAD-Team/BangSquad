// Source/Project_Bang_Squad/Character/StageBoss/Stage2Boss.h

#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBossData.h"
#include "Stage2Boss.generated.h"

// 전방 선언
class AEnemySpawner;
class UHealthComponent;
class UAnimMontage;

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
    void PerformSmashAttack(AActor* Target);

    // --- [Phase & QTE] ---
    bool IsInvinciblePhase() const { return bIsInvincible; }
    void CheckHealthPhase();
    void StartQTEPattern(AActor* Target);

    // [Getter]
    UEnemyBossData* GetBossData() const { return BossData; }

    // [QTE 결과 처리]
    // 클라이언트의 RPC를 받은 PlayerController가 이 함수를 호출합니다.
    UFUNCTION(BlueprintCallable)
    void OnQTEResult(bool bSuccess);

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
};