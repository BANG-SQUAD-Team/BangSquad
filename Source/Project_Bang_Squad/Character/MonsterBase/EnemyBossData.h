#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBaseData.h" 
#include "Project_Bang_Squad/Projectile/SlashProjectile.h"
// [NEW] 마법 투사체 클래스 헤더 추가 (전방 선언도 가능하지만, TSubclassOf 안전성을 위해 포함 권장)
#include "Project_Bang_Squad/Projectile/MageProjectile.h" 
#include "EnemyBossData.generated.h"

/**
 * [Boss Class]
 * 통합 보스 데이터 에셋 (Uber-Data Asset)
 * - Stage 1 (기사), Stage 2 (마법사) 등 모든 보스의 데이터를 담을 수 있는 그릇입니다.
 * - 사용하는 보스에 맞춰 필요한 섹션만 채워서 사용합니다.
 */
UCLASS(BlueprintType)
class PROJECT_BANG_SQUAD_API UEnemyBossData : public UEnemyBaseData
{
    GENERATED_BODY()

public:
    // =============================================================
    // [1] 공통 데이터 (Common) - 모든 보스가 사용하는 항목
    // =============================================================

    // 기믹 발동 체력 비율 (0.5 = 50% 체력일 때 기믹 발동)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Common", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float GimmickThresholdRatio = 0.5f;

    // 어그로 발견 시 최초 1회 재생 (포효)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Common")
    TObjectPtr<UAnimMontage> AggroMontage;

    // 기본 공격 동작들 (랜덤 재생)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Common")
    TArray<TObjectPtr<UAnimMontage>> AttackMontages;

    // 피격 리액션 (맞았을 때)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Common")
    TObjectPtr<UAnimMontage> HitReactMontage;

    // 사망 애니메이션
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Common")
    TObjectPtr<UAnimMontage> DeathMontage;

    // QTE 패턴 시작 전조 동작 (예: 하늘 가리키기)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Common")
    TObjectPtr<UAnimMontage> QTE_TelegraphMontage;

    // 데스 월(죽음의 벽) 소환 패턴 (보스 공통 기믹이라면 여기에 위치)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Common")
    TObjectPtr<UAnimMontage> DeathWallSummonMontage;


    // =============================================================
    // [2] Stage 1 전용 (Knight) - 칼 쓰는 보스용
    // =============================================================

    // 참격 투사체 클래스
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage1_Knight")
    TSubclassOf<ASlashProjectile> SlashProjectileClass;

    // 참격 발사 몽타주 (칼 휘두르기)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage1_Knight")
    TObjectPtr<UAnimMontage> SlashAttackMontage;

    // 스파이크(가시) 패턴용 몽타주 (Stage 1 전용이라고 가정)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage1_Knight")
    TObjectPtr<UAnimMontage> SpellMontage;


    // =============================================================
    // [3] Stage 2 전용 (Mage) - 마법 쓰는 보스용 [NEW!]
    // =============================================================

    // [NEW] 마법 구체 투사체 (유도탄 등)
    // 업로드해주신 파일 리스트에 있는 MageProjectile을 활용
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage2_Mage")
    TSubclassOf<AMageProjectile> MagicProjectileClass;

    // [NEW] 마법 발사 몽타주 (지팡이 휘두르기)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage2_Mage")
    TObjectPtr<UAnimMontage> MagicAttackMontage;

    // [NEW] 순간이동 몽타주 (마법사 보스의 핵심 이동기)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage2_Mage")
    TObjectPtr<UAnimMontage> TeleportMontage;

    // [NEW] 광역기 시전 몽타주 (하늘에서 메테오 등)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stage2_Mage")
    TObjectPtr<UAnimMontage> AreaSkillMontage;
};