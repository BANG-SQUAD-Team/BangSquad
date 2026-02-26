#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBaseData.h" 
#include "Project_Bang_Squad/Projectile/SlashProjectile.h"
#include "Project_Bang_Squad/Projectile/MageProjectile.h" 
#include "EnemyBossData.generated.h"

// =============================================================
// [추가] Stage 3 Boss 스킬 열거형 및 세부 설정 구조체
// UCLASS보다 무조건 위에 있어야 에러가 나지 않습니다.
// =============================================================
UENUM(BlueprintType)
enum class EBoss3Skill : uint8
{
    Basic, Laser, Meteor, Break, Chase
};

USTRUCT(BlueprintType)
struct FBoss3SkillDetails
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    float Cooldown = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    float Weight = 50.0f;
};

/**
 * [Boss Class]
 * 보스 공통 데이터 관리
 */
UCLASS(BlueprintType)
class PROJECT_BANG_SQUAD_API UEnemyBossData : public UEnemyBaseData
{
    GENERATED_BODY()

public:
    // =============================================================
    // [1] 공통 데이터 (Common)
    // =============================================================
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Common", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float GimmickThresholdRatio = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|UI")
    UTexture2D* BossIcon;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|UI")
    FText BossName;

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
    // [2] Stage 1 보스 (Knight)
    // =============================================================
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage1_Knight")
    TSubclassOf<ASlashProjectile> SlashProjectileClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage1_Knight")
    TObjectPtr<UAnimMontage> SlashAttackMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage1_Knight")
    TObjectPtr<UAnimMontage> SpellMontage;

    // =============================================================
    // [3] Stage 2 보스 (Mage)
    // =============================================================
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Mage")
    TSubclassOf<AActor> MagicProjectileClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Mage")
    TObjectPtr<UAnimMontage> MagicAttackMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Mage")
    TObjectPtr<UAnimMontage> TeleportMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Mage")
    TObjectPtr<UParticleSystem> TeleportVFX;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Mage")
    TObjectPtr<UAnimMontage> MeleeAttackMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Mage")
    float MeleeAttackRadius = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Mage")
    float MeleeAttackDamage = 30.0f;

    // =============================================================
    // [4] Stage 2 보스 (Spider)
    // =============================================================
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage2_Spider")
    TSubclassOf<AActor> WebProjectileClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage2_Spider")
    TObjectPtr<UAnimMontage> WebShotMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage2_Spider")
    TObjectPtr<UAnimMontage> QTEAttackMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage2_Spider")
    TObjectPtr<UAnimMontage> WipePatternMontage;

    // =============================================================
    // [5] Stage 3 미드보스
    // =============================================================
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage3_MidBoss")
    TSubclassOf<AActor> Stage3RangedProjectileClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage3_MidBoss")
    TObjectPtr<UAnimMontage> Stage3RangedAttackMontage;

    // =============================================================
    // [6] Stage 3 보스
    // =============================================================

    // [추가] 컨트롤러에서 사용할 스킬 설정 맵
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Stage3|AI")
    TMap<EBoss3Skill, FBoss3SkillDetails> Stage3SkillConfigs;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage3")
    TSubclassOf<AActor> MeteorProjectileClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage3")
    TSubclassOf<AActor> WindOrbClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage3")
    TObjectPtr<UAnimMontage> WingAttackL_Montage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage3")
    TObjectPtr<UAnimMontage> WingAttackR_Montage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage3")
    TObjectPtr<UAnimMontage> LaserMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage3")
    TObjectPtr<UAnimMontage> MeteorMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage3")
    TObjectPtr<UAnimMontage> JumpAttackMontage;
};