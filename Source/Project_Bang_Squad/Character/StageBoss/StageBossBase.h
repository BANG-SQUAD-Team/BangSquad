// Source/Project_Bang_Squad/Character/StageBoss/StageBossBase.h

#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h"
#include "StageBossBase.generated.h"

/** 보스 페이즈 정의 */
UENUM(BlueprintType)
enum class EBossPhase : uint8
{
    Intro       UMETA(DisplayName = "Intro"),
    Phase1      UMETA(DisplayName = "Phase 1 (Combat)"),
    Gimmick     UMETA(DisplayName = "Gimmick (Invincible)"),
    Phase2      UMETA(DisplayName = "Phase 2 (Enraged)"),
    Dead        UMETA(DisplayName = "Dead")
};

UCLASS()
class PROJECT_BANG_SQUAD_API AStageBossBase : public AEnemyCharacterBase
{
    GENERATED_BODY()

public:
    AStageBossBase();

    // [Replication] 변수 복제 등록
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // 데미지 받는 함수 오버라이드 (무적 처리용 - 오직 서버에서만 실행됨)
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

    /** 기믹 해결 시 호출 (수정 파괴 등) */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Gimmick")
    virtual void OnGimmickResolved(int32 GimmickID);

    /** * 왜 이렇게 짰는가: AI Controller가 이 값을 보고 새로운 패턴을 넘길지 말지 결정합니다.
     * Replicated를 통해 클라이언트도 보스가 행동 중임을 인지할 수 있습니다.
     */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "AI")
    bool bIsActionInProgress = false;

    /** 왜 이렇게 짰는가: 몽타주가 끝났을 때 AI를 다시 활성화하기 위한 서버 함수 (권한 보장) */
    UFUNCTION(Server, Reliable)
    void ServerSetActionInProgress(bool bInProgress);

protected:
    // 페이즈 변경 (서버 전용)
    void SetPhase(EBossPhase NewPhase);

    // 페이즈 변경 시 로직 (자식 클래스 구현용)
    virtual void OnPhaseChanged(EBossPhase NewPhase);

    // --- [State] ---
    /** 페이즈 동기화: 값이 변하면 OnRep_CurrentPhase가 클라이언트에서 호출됨 */
    UPROPERTY(ReplicatedUsing = OnRep_CurrentPhase, VisibleAnywhere, BlueprintReadOnly, Category = "Boss|State")
    EBossPhase CurrentPhase;

    UFUNCTION()
    void OnRep_CurrentPhase();

    /** 무적 상태 (True면 데미지 0) */
    UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Boss|State")
    bool bIsInvincible = false;

    // 현재 기믹 진행도를 체크하기 위한 카운트 (서버 전용 내부 변수)
    int32 RemainingGimmickCount = 0;
};