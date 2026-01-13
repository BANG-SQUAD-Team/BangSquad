// EnemyMidBoss.h

#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h" // 부모 클래스
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBossData.h"      // 데이터 에셋
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"      // 헬스 컴포넌트
#include "EnemyMidBoss.generated.h"

// 보스 페이즈 관리용 Enum
UENUM(BlueprintType)
enum class EMidBossPhase : uint8
{
    Normal      UMETA(DisplayName = "Normal Phase"),
    Gimmick     UMETA(DisplayName = "Gimmick Phase"),
    Enraged     UMETA(DisplayName = "Enraged Phase"),
    Dead        UMETA(DisplayName = "Dead")
};

/**
 * [EnemyMidBoss] 통합 버전
 * 1. Data Asset 기반 외형/스탯 초기화
 * 2. Network Phase 동기화 (RepNotify)
 * 3. HealthComponent & AIController 연동 (TakeDamage)
 * 4. Animation Montage 헬퍼 함수 제공
 */
UCLASS()
class PROJECT_BANG_SQUAD_API AEnemyMidBoss : public AEnemyCharacterBase
{
    GENERATED_BODY()

public:
    AEnemyMidBoss();

    // [Network] 변수 복제 설정
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // [Editor] 외형 실시간 갱신
    virtual void OnConstruction(const FTransform& Transform) override;

    // [Combat] 데미지 처리 (HealthComponent + AI 알림)
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

    /* --- [Animation Helpers] --- */
    // AI Controller에서 쉽게 호출하기 위한 헬퍼 함수들
    float PlayAggroAnim();      // 포효
    float PlayRandomAttack();   // 랜덤 공격
    float PlayHitReactAnim();   // 피격 리액션

protected:
    virtual void BeginPlay() override;

    /* --- [Phase Control] --- */
public:
    // 서버에서 페이즈 변경 시 호출
    void SetPhase(EMidBossPhase NewPhase);

    // 현재 페이즈 가져오기 (Getter)
    EMidBossPhase GetPhase() const { return CurrentPhase; }

protected:
    // --- [Components] ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UHealthComponent> HealthComponent;

    // --- [Data Asset] ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Data")
    TObjectPtr<UEnemyBossData> BossData;

    // --- [State (Replicated)] ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentPhase, Category = "Boss|State")
    EMidBossPhase CurrentPhase;

    // 클라이언트 연출 동기화 함수
    UFUNCTION()
    void OnRep_CurrentPhase();
};