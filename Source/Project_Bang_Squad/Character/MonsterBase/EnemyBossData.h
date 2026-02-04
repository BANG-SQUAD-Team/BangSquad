#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBaseData.h" 
#include "Project_Bang_Squad/Projectile/SlashProjectile.h"
#include "Project_Bang_Squad/Projectile/MageProjectile.h" 
#include "EnemyBossData.generated.h"

/**
 * [Boss Class]
 * 통합 보스 데이터 에셋
 * 주의: AttackRange, AttackDamage, MaxHealth 등은 부모(UEnemyBaseData)에 있으므로 여기서 선언 금지!
 */
UCLASS(BlueprintType)
class PROJECT_BANG_SQUAD_API UEnemyBossData : public UEnemyBaseData
{
    GENERATED_BODY()

public:
    // =============================================================
    // [1] 공통 데이터 (Common)
    // =============================================================

    // 기믹 발동 체력 비율
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Common", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float GimmickThresholdRatio = 0.5f;

    // 어그로 / 피격 / 사망 몽타주
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Common")
    TObjectPtr<UAnimMontage> AggroMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Common")
    TArray<TObjectPtr<UAnimMontage>> AttackMontages;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Common")
    TObjectPtr<UAnimMontage> HitReactMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Common")
    TObjectPtr<UAnimMontage> DeathMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Common")
    TObjectPtr<UAnimMontage> QTE_TelegraphMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Common")
    TObjectPtr<UAnimMontage> DeathWallSummonMontage;

    // =============================================================
    // [2] Stage 1 전용 (Knight)
    // =============================================================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage1_Knight")
    TSubclassOf<ASlashProjectile> SlashProjectileClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage1_Knight")
    TObjectPtr<UAnimMontage> SlashAttackMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage1_Knight")
    TObjectPtr<UAnimMontage> SpellMontage;

    // =============================================================
    // [3] Stage 2 전용 (Mage)
    // =============================================================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage2_Mage")
    TSubclassOf<AMageProjectile> MagicProjectileClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage2_Mage")
    TObjectPtr<UAnimMontage> MagicAttackMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage2_Mage")
    TObjectPtr<UAnimMontage> TeleportMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage2_Mage")
    TObjectPtr<UAnimMontage> AreaSkillMontage;
};