// Source/Project_Bang_Squad/Character/StageBoss/Stage1Boss.h

#pragma once

#include "CoreMinimal.h"
#include "StageBossBase.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBossData.h"
#include "Project_Bang_Squad/Core/BSGameInstance.h"
#include "Engine/TargetPoint.h"
#include "AITypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "Project_Bang_Squad/BossPattern/Boss1_Rampart.h"
#include "Stage1Boss.generated.h"

// 전방 선언
class AJobCrystal;
class ASlashProjectile;
class UAnimMontage;
class ADeathWall;
class ABossSpikeTrap;
class UBoxComponent;

struct FPlayerQTEStatus
{
    int32 PressCount = 0;
    bool bFailed = false;
};

/**
 * Stage 1 Boss: Rampart Gimmick, Death Wall, Spear QTE, Heavy Melee Attack
 */
UCLASS()
class PROJECT_BANG_SQUAD_API AStage1Boss : public AStageBossBase
{
    GENERATED_BODY()

public:
    AStage1Boss();

protected:
    virtual void BeginPlay() override;

    // --- [Data Asset Config] ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Data")
    TObjectPtr<UEnemyBossData> BossData;


    // ==============================================================================
    // [1] 일반 공격 설정 (Melee Combat)
    // ==============================================================================
public:
    UPROPERTY(EditAnywhere, Category = "Boss|Combat")
    float MeleeDamageAmount = 30.0f; // 기본 공격 데미지

    // [수정됨] 이름 충돌 방지를 위해 변수명 변경 (MeleeHitBox -> MeleeCollisionBox)
    // 블루프린트의 컴포넌트를 자동으로 찾아서 연결할 것입니다.
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Boss|Combat")
    TObjectPtr<UBoxComponent> MeleeCollisionBox;

protected:
    // 공격 선딜레이(3초)를 제어할 타이머 핸들
    FTimerHandle MeleeAttackTimerHandle;

    // 3초 뒤에 애니메이션 재생 속도를 복구하는 함수
    void ReleaseMeleeAttackHold();

    // 모든 클라이언트의 보스 애니메이션 속도를 동기화하여 조절 (멈춤/재생)
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_SetAttackPlayRate(float NewRate);

public:
    // [노티파이 1] 칼을 높게 든 지점에서 호출 (속도 0으로 만들어 3초 대기)
    UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
    void AnimNotify_StartMeleeHold();

    // [노티파이 2] 실제로 내리치는 지점에서 호출 (박스 컴포넌트 판정)
    UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
    void AnimNotify_ExecuteMeleeHit();

    // AI 명령용 공격 함수들
    UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
    void DoAttack_Slash();

    UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
    void DoAttack_Swing();


    // ==============================================================================
    // [2] 기믹 설정 (Crystals & Death Wall & Rampart)
    // ==============================================================================
public:
    UPROPERTY(EditDefaultsOnly, Category = "Boss|Gimmick")
    TMap<EJobType, TSubclassOf<AJobCrystal>> JobCrystalClasses;

    UPROPERTY(EditInstanceOnly, Category = "Boss|Gimmick")
    TArray<TObjectPtr<ATargetPoint>> CrystalSpawnPoints;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Gimmick")
    TSubclassOf<ADeathWall> DeathWallClass;

    UPROPERTY(EditInstanceOnly, Category = "Boss|Gimmick")
    TObjectPtr<ATargetPoint> DeathWallCastLocation;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Gimmick")
    float DeathWallPatternDuration = 60.0f; // 보스가 묶여있는 시간

protected:
    // 성벽(DeathWall) 패턴 상태 변수
    bool bHasTriggeredDeathWall = false;
    bool bIsDeathWallSequenceActive = false;

    // 타이머 핸들
    FTimerHandle DeathWallTimerHandle; // 보스 패턴 종료용
    FTimerHandle FailSafeTimerHandle;  // 비상용
    FTimerHandle RampartTimerHandle;   // 성벽 복구용

    // 성벽 패턴 함수
    void StartDeathWallSequence();
    void SpawnDeathWall();
    void FinishDeathWallPattern();

    // 성벽(Rampart) 제어 함수
    void ControlRamparts(bool bSink); // true: 내림, false: 올림
    void RestoreRamparts();           // 1분 45초 뒤 호출

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayDeathWallMontage();

    // 애니메이션 노티파이: 벽 소환 시점
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Combat")
    void AnimNotify_ActivateDeathWall();


    // ==============================================================================
    // [3] QTE 시스템 (Spear QTE)
    // ==============================================================================
public:
    UPROPERTY(EditDefaultsOnly, Category = "Boss|QTE")
    float QTEDuration = 3.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|QTE")
    int32 QTEGoalCount = 10;

    UFUNCTION(BlueprintCallable, Category = "Boss|QTE")
    void StartSpearQTE();

    UFUNCTION(Server, Reliable, BlueprintCallable)
    void Server_SubmitQTEInput(APlayerController* PlayerController);

private:
    TMap<APlayerController*, FPlayerQTEStatus> QTEProgressMap;
    FTimerHandle QTETimerHandle;

    void EndSpearQTE();
    void PerformWipeAttack(); // QTE 실패 시 전멸기

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_SetQTEWidget(bool bVisible);


    // ==============================================================================
    // [4] 스파이크(가시) 패턴 & 기타
    // ==============================================================================
public:
    UFUNCTION(BlueprintCallable, Category = "Boss Pattern")
    void StartSpikePattern();

    UFUNCTION(BlueprintCallable, Category = "Boss Pattern")
    void ExecuteSpikeSpell();

    UFUNCTION(BlueprintCallable, Category = "Boss Pattern")
    void TrySpawnSpikeAtRandomPlayer();

protected:
    UPROPERTY(EditAnywhere, Category = "Boss Pattern")
    TSubclassOf<class ABossSpikeTrap> SpikeTrapClass;

    UPROPERTY(EditAnywhere, Category = "Boss Pattern")
    TObjectPtr<UAnimMontage> SpellMontage;

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlaySpellMontage();


    // ==============================================================================
    // [5] 공용 오버라이드 및 유틸리티
    // ==============================================================================
protected:
    virtual void OnPhaseChanged(EBossPhase NewPhase) override;
    virtual void OnGimmickResolved(int32 GimmickID) override;
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
    virtual void OnDeathStarted() override;

    bool bPhase2Started = false;
    void EnterPhase2();
    void SpawnCrystals();

    // 몽타주 재생 헬퍼
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayAttackMontage(UAnimMontage* MontageToPlay, FName SectionName = NAME_None);

    UFUNCTION()
    void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    // AI 이동 완료 콜백
    UFUNCTION()
    void OnArrivedAtCastLocation(FAIRequestID RequestID, EPathFollowingResult::Type Result);

    // 체력 변경 콜백
    UFUNCTION()
    void OnHealthChanged(float CurrentHealth, float MaxHealth);
};