#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBaseData.h" 
#include "Project_Bang_Squad/Projectile/SlashProjectile.h"
#include "Project_Bang_Squad/Projectile/MageProjectile.h" 
#include "EnemyBossData.generated.h"

/**
 * [Boss Class]
 * ���� ���� ������ ����
 * ����: AttackRange, AttackDamage, MaxHealth ���� �θ�(UEnemyBaseData)�� �����Ƿ� ���⼭ ���� ����!
 */
UCLASS(BlueprintType)
class PROJECT_BANG_SQUAD_API UEnemyBossData : public UEnemyBaseData
{
    GENERATED_BODY()

public:
    // =============================================================
    // [1] ���� ������ (Common)
    // =============================================================

    // ��� �ߵ� ü�� ����
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Common", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float GimmickThresholdRatio = 0.5f;

    // 보스 체력바에 표시될 얼굴 이미지
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|UI")
    UTexture2D* BossIcon;
    
    // 보스 체력바 위에 표시될 보스 이름
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|UI")
    FText BossName;
    
    // ��׷� / �ǰ� / ��� ��Ÿ��
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
    // [2] Stage 1 ���� (Knight)
    // =============================================================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage1_Knight")
    TSubclassOf<ASlashProjectile> SlashProjectileClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage1_Knight")
    TObjectPtr<UAnimMontage> SlashAttackMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage1_Knight")
    TObjectPtr<UAnimMontage> SpellMontage;

    // =============================================================
    // [3] Stage 2 ���� (Mage)
    // =============================================================

// 1. ���Ÿ� ����
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Mage")
    TSubclassOf<AActor> MagicProjectileClass; // ����ü Ŭ����

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Mage")
    TObjectPtr<UAnimMontage> MagicAttackMontage; // �߻� ���

    // 2. �ڷ���Ʈ
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Mage")
    TObjectPtr<UAnimMontage> TeleportMontage; // ������� ��Ÿ���� ���

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Mage")
    TObjectPtr<UParticleSystem> TeleportVFX; // �ڷ���Ʈ ����Ʈ

    // 3. ���� ���� (Sphere Trace)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Mage")
    TObjectPtr<UAnimMontage> MeleeAttackMontage; // �ֵθ��� ���

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Mage")
    float MeleeAttackRadius = 150.0f; // ���� ���� (������)

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Mage")
    float MeleeAttackDamage = 30.0f; // ���� ������


    // =============================================================
    // [4] Stage 2 ���� (Spider - ���⸦ �߰��ؾ� ������ �ذ�˴ϴ�!)
    // =============================================================

    // �Ź��� ����ü Ŭ���� (BP_WebProjectile �Ҵ��)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage2_Spider")
    TSubclassOf<AActor> WebProjectileClass;

    // �Ź��� �߻� ��Ÿ��
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage2_Spider")
    TObjectPtr<UAnimMontage> WebShotMontage;

    // ��� ���(QTE) ���� ��Ÿ��
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage2_Spider")
    TObjectPtr<UAnimMontage> QTEAttackMontage;

    // ����� ��ȿ ��Ÿ��
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage2_Spider")
    TObjectPtr<UAnimMontage> WipePatternMontage;

    // =============================================================
    // [5] Stage 3 미드보스 (기본 평타 + 원거리 공격)
    // =============================================================

    // 원거리 공격용 투사체 클래스
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage3_MidBoss")
    TSubclassOf<AActor> Stage3RangedProjectileClass;

    // 원거리 공격 발동 몽타주
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage3_MidBoss")
    TObjectPtr<UAnimMontage> Stage3RangedAttackMontage;

};