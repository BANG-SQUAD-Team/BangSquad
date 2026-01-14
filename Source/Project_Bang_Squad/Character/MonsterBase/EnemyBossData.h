#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBaseData.h" 
#include "Project_Bang_Squad/Projectile/SlashProjectile.h"
#include "EnemyBossData.generated.h"

/**
 * [Boss Class]
 * EnemyBaseData를 상속받아, 보스(중간보스 & 스테이지보스)에게만 필요한 데이터를 추가.
 */
UCLASS(BlueprintType)
class PROJECT_BANG_SQUAD_API UEnemyBossData : public UEnemyBaseData
{
    GENERATED_BODY()

public:
    // --- [Boss Specific Config] ---

    // 기믹 발동 체력 비율 (0.5 = 50% 체력일 때 기믹 발동)
    // 0.0 ~ 1.0 사이의 값만 입력되도록 제한(Clamp)합니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float GimmickThresholdRatio = 0.5f;

    // 1. 어그로 발견 시 최초 1회 재생 (포효)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Anim")
    TObjectPtr<UAnimMontage> AggroMontage;

    // 2. 공격 동작들 (랜덤 재생)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Anim")
    TArray<TObjectPtr<UAnimMontage>> AttackMontages;

    // 3. 피격 리액션 (맞았을 때)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Anim")
    TObjectPtr<UAnimMontage> HitReactMontage;

    // 4. 사망 애니메이션 (죽었을 때 재생할 몽타주)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Anim")
    TObjectPtr<UAnimMontage> DeathMontage;

    // 5. 참격 투사체 클래스 (여기에 BP_SlashProjectile을 넣을 예정)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Combat")
    TSubclassOf<ASlashProjectile> SlashProjectileClass;

    // 6. 참격 발사 몽타주 (칼을 휘두르는 애니메이션)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Anim")
    TObjectPtr<UAnimMontage> SlashAttackMontage;
    
    // (추후 확장)
    // 보스 등장 몽타주, 전용 BGM 등은 여기에 추가하면 됩니다.
};