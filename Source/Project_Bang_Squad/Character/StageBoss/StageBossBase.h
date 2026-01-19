#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h"
#include "StageBossBase.generated.h"

// 보스 페이즈 정의
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
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // 데미지 받는 함수 오버라이드 (무적 처리용)
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

    // 기믹 해결 시 호출 (수정 파괴 등)
// BlueprintAuthorityOnly: 블루프린트 노드에 "서버 전용(서버 아이콘)" 표시가 뜨게 합니다.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Gimmick") 
        virtual void OnGimmickResolved(int32 GimmickID);

protected:
    // 페이즈 변경 (서버 전용)
    void SetPhase(EBossPhase NewPhase);

    // 페이즈 변경 시 로직 (자식 클래스 구현용)
    virtual void OnPhaseChanged(EBossPhase NewPhase);

    // --- [State] ---
    UPROPERTY(ReplicatedUsing = OnRep_CurrentPhase, VisibleAnywhere, BlueprintReadOnly, Category = "Boss|State")
    EBossPhase CurrentPhase;

    UFUNCTION()
    void OnRep_CurrentPhase();

    // 무적 상태 (True면 데미지 0)
    UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Boss|State")
    bool bIsInvincible = false;

    // 현재 기믹 진행도를 체크하기 위한 카운트
    int32 RemainingGimmickCount = 0;
};