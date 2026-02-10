// Source/Project_Bang_Squad/Character/Enemy/Stage2MidBoss.h

#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBossData.h"
#include "Stage2MidBoss.generated.h"

class UHealthComponent;
class UAnimMontage;

// [Stage 2 보스 전용 페이즈]
UENUM(BlueprintType)
enum class EStage2Phase : uint8
{
    Normal      UMETA(DisplayName = "Normal Phase"),
    Gimmick     UMETA(DisplayName = "Gimmick Phase (Teleport)"),
    Enraged     UMETA(DisplayName = "Enraged Phase (Area Skill)"),
    Dead        UMETA(DisplayName = "Dead")
};

/**
 * [Stage 2 Mid Boss - Mage Type]
 * 특징: 검(Sword) 대신 원거리 마법(Magic)과 순간이동(Teleport) 사용
 */
UCLASS()
class PROJECT_BANG_SQUAD_API AStage2MidBoss : public AEnemyCharacterBase
{
    GENERATED_BODY()

public:
    AStage2MidBoss();
    UEnemyBossData* GetBossData() const { return BossData; }

    // [Replication] 페이즈 동기화 등록
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // [Editor] 에디터 값 변경 시 호출
    virtual void OnConstruction(const FTransform& Transform) override;

    // [Combat] 데미지 처리
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

    /* --- [AI Controllable Interface] --- */
    // AI 컨트롤러가 이 함수들을 호출하여 보스를 조종합니다.

    // 1. [Melee] 지팡이 휘두르기
    UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
    void PerformAttackTrace();

    // 2. [Range] 유도탄 마법 발사
    UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
    void FireMagicMissile();

    // 3. [Pattern] 광역기 패턴
    UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
    void CastAreaSkill();

    // 4. [Utility] 타겟 근처로 텔레포트 시도 (성공 시 true 반환)
    bool TryTeleportToTarget(AActor* Target, float DistanceFromTarget);

    /* --- [Animation Helpers] --- */
    // 몽타주 재생 후 재생 시간(Duration)을 반환하여 AI가 대기할 시간을 알려줌
    float PlayMeleeAttackAnim();
    float PlayMagicAttackAnim();
    float PlayTeleportAnim();

protected:
    virtual void BeginPlay() override;

    // [Visual Sync] 모든 클라이언트에게 몽타주 재생 명령 (Unreliable 권장)
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayMontage(UAnimMontage* MontageToPlay);

    // [Visual Sync] 텔레포트 이펙트 방송
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_TeleportEffect(FVector Location);

    // [State Control]
    void SetPhase(EStage2Phase NewPhase);

    UFUNCTION()
    void OnRep_CurrentPhase();

    UFUNCTION()
    void OnDeath();

protected:
    // --- [Components] ---
    // 부모(EnemyCharacterBase)에 HealthComponent가 없다면 유지, 있다면 제거
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UHealthComponent> HealthComponent;

    // --- [Data Asset] ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Data")
    TObjectPtr<UEnemyBossData> BossData;

    // --- [Combat Setup - Blueprints] ---
    /** 마법 발사 몽타주 */
    UPROPERTY(EditDefaultsOnly, Category = "Boss|Animation")
    TObjectPtr<UAnimMontage> MagicAttackMontage;

    /** 근접 공격 몽타주 */
    UPROPERTY(EditDefaultsOnly, Category = "Boss|Animation")
    TObjectPtr<UAnimMontage> MeleeAttackMontage;

    /** 텔레포트 몽타주 (사라질 때/나타날 때) */
    UPROPERTY(EditDefaultsOnly, Category = "Boss|Animation")
    TObjectPtr<UAnimMontage> TeleportMontage;

    /** 마법 투사체 클래스 (BP_MageProjectile) */
    UPROPERTY(EditDefaultsOnly, Category = "Boss|Combat")
    TSubclassOf<AActor> MagicProjectileClass;

    /** 텔레포트 파티클 이펙트 */
    UPROPERTY(EditDefaultsOnly, Category = "Boss|VFX")
    TObjectPtr<UParticleSystem> TeleportEffect;

    // --- [Combat Stats] ---
    UPROPERTY(EditAnywhere, Category = "Boss|Stat")
    float AttackRange = 150.0f;

    UPROPERTY(EditAnywhere, Category = "Boss|Stat")
    float AttackRadius = 50.0f;

    // --- [State] ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentPhase, Category = "Boss|State")
    EStage2Phase CurrentPhase;
};