// Source/Project_Bang_Squad/Character/StageBoss/Stage1Boss.h
#pragma once

#include "CoreMinimal.h"
#include "StageBossBase.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBossData.h" // [NEW] 데이터 에셋 헤더 추가
#include "Stage1Boss.generated.h"

// 전방 선언
class AJobCrystal;
class ASlashProjectile;
class UAnimMontage;
class ADeathWall;

// 각 플레이어의 QTE 진행 상황 저장 구조체
struct FPlayerQTEStatus
{
    int32 PressCount = 0;
    bool bFailed = false;
};

UCLASS()
class PROJECT_BANG_SQUAD_API AStage1Boss : public AStageBossBase
{
    GENERATED_BODY()

public:
    AStage1Boss();

protected:
    virtual void BeginPlay() override;

    // --- [Data Asset Config] ---
public:
    // [NEW] 보스 공용 데이터 (공격 모션, 투사체, 기믹 발동 비율 등)
    // 기획자가 여기서 데이터 에셋(DA_Stage1Boss)을 할당합니다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Data")
    TObjectPtr<UEnemyBossData> BossData;

    // --- [Stage Specific Config: 1스테이지 전용 기믹] ---
    // 아래 항목들은 모든 보스 공통이 아니라, 1스테이지 보스만의 특수 기믹이므로 여기에 남겨둡니다.
public:
    UPROPERTY(EditDefaultsOnly, Category = "Boss|Gimmick")
    TSubclassOf<AJobCrystal> CrystalClass;

    UPROPERTY(EditAnywhere, Category = "Boss|Gimmick", meta = (MakeEditWidget = true))
    TArray<FVector> CrystalSpawnPoints;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Gimmick")
    TSubclassOf<ADeathWall> DeathWallClass;

    UPROPERTY(EditAnywhere, Category = "Boss|Gimmick", meta = (MakeEditWidget = true))
    FVector WallSpawnLocation;

    // --- [Combat Config: 전투 수치] ---
    // (참고: 데미지 등도 나중에는 BaseData로 옮기는 것이 좋으나, 우선 여기에 둡니다)
    UPROPERTY(EditAnywhere, Category = "Boss|Combat")
    float MeleeAttackRadius = 400.0f;

    UPROPERTY(EditAnywhere, Category = "Boss|Combat")
    float MeleeDamageAmount = 30.0f;

    // --- [QTE Config] ---
    UPROPERTY(EditDefaultsOnly, Category = "Boss|QTE")
    float QTEDuration = 3.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|QTE")
    int32 QTEGoalCount = 10;

protected:
    // --- [Override Functions] ---
    virtual void OnPhaseChanged(EBossPhase NewPhase) override;
    virtual void OnGimmickResolved(int32 GimmickID) override;

    // 데미지 체크 (페이즈 2 진입용)
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

    // 사망 처리 (승리 로직)
    virtual void OnDeathStarted() override;

    // --- [AI Command Interface] ---
public:
    UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
    void DoAttack_Slash();

    UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
    void DoAttack_Swing();

    // QTE 패턴 시작
    UFUNCTION(BlueprintCallable, Category = "Boss|QTE")
    void StartSpearQTE();

    // 클라이언트 입력 수신 (Client -> Server)
    UFUNCTION(Server, Reliable, BlueprintCallable)
    void Server_SubmitQTEInput(APlayerController* PlayerController);

    // 애니메이션 노티파이에서 호출: 실제 투사체 발사
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Combat")
    void AnimNotify_SpawnSlash();
    // --- [AnimNotify Hooks] ---
protected:

    // 애니메이션 노티파이에서 호출: 근접 공격 판정
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Combat")
    void AnimNotify_CheckMeleeHit();

    // --- [Phase Logic] ---
protected:
    bool bPhase2Started = false;
    void EnterPhase2();

    // --- [Internal Logic] ---
private:
    void SpawnCrystals();
    void SpawnDeathWall();

    // [Refactor] 이제 몽타주를 직접 인자로 받습니다.
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayAttackMontage(UAnimMontage* MontageToPlay, FName SectionName = NAME_None);

    // QTE 관련 내부 함수
    void EndSpearQTE();
    void PerformWipeAttack();

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_SetQTEWidget(bool bVisible);

    TMap<APlayerController*, FPlayerQTEStatus> QTEProgressMap;
    FTimerHandle QTETimerHandle;
};