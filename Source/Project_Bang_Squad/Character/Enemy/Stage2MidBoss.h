// Source/Project_Bang_Squad/Character/Enemy/Stage2MidBoss.h

#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBossData.h"
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"
// [절대 규칙] generated.h는 다른 모든 include보다 항상 가장 아래에 있어야 합니다.
#include "Stage2MidBoss.generated.h"

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

    // [Replication] 페이즈 동기화
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // [Data Driven] 에디터 수정 사항 즉시 반영
    virtual void OnConstruction(const FTransform& Transform) override;

    // 데미지 처리 (AI 어그로 연결)
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

    /* --- [Combat Interface] --- */

    // 1. [Sphere Trace] 지팡이 휘두르기 (일반 공격)
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void PerformAttackTrace();

    // 2. [Projectile] 유도탄 마법 발사
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void FireMagicMissile();

    // 3. [Pattern] 광역기 패턴
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void CastAreaSkill();

    /* --- [Animation Helpers] --- */
    float PlayMagicAttackAnim();
    float PlayTeleportAnim();

protected:
    virtual void BeginPlay() override;

    // [Visual Sync] 모든 클라이언트에게 몽타주 재생 명령
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayMontage(UAnimMontage* MontageToPlay);

    // [Server Logic] 실제 투사체 생성 (서버만 실행)
    UFUNCTION(Server, Reliable)
    void Server_SpawnMagicProjectile(FVector SpawnLoc, FRotator SpawnRot);

    // [State Control]
    void SetPhase(EStage2Phase NewPhase);

    UFUNCTION()
    void OnRep_CurrentPhase();

    UFUNCTION()
    void OnDeath();

protected:
    // --- [Components] ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UHealthComponent> HealthComponent;

    // --- [Data Asset] ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Data")
    TObjectPtr<UEnemyBossData> BossData;

    // --- [Combat Stats] ---
    UPROPERTY(EditAnywhere, Category = "Combat")
    float AttackRange = 150.0f;

    UPROPERTY(EditAnywhere, Category = "Combat")
    float AttackRadius = 50.0f;

    // --- [State] ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentPhase, Category = "Boss|State")
    EStage2Phase CurrentPhase;
};