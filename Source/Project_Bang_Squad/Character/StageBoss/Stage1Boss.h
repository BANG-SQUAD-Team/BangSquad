// Source/Project_Bang_Squad/Character/StageBoss/Stage1Boss.h
#pragma once

#include "CoreMinimal.h"
#include "StageBossBase.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBossData.h"
#include "Project_Bang_Squad/Core/BSGameInstance.h"
#include "Engine/TargetPoint.h" // [New] 고정 위치 스폰을 위해 추가
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
    // 보스 공용 데이터 (공격 모션, 투사체 등)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Data")
    TObjectPtr<UEnemyBossData> BossData;

    // --- [Stage Specific Config: 1스테이지 전용 기믹] ---
public:
    // 직업별로 소환될 크리스탈 블루프린트 매핑
    UPROPERTY(EditDefaultsOnly, Category = "Boss|Gimmick")
    TMap<EJobType, TSubclassOf<AJobCrystal>> JobCrystalClasses;

    // [Refactored] 기존 위젯(상대 좌표) 대신 레벨에 배치된 타겟 포인트(절대 좌표) 사용
    // EditInstanceOnly: 블루프린트 원본이 아닌, 레벨에 배치된 인스턴스에서만 설정 가능
    UPROPERTY(EditInstanceOnly, Category = "Boss|Gimmick")
    TArray<TObjectPtr<ATargetPoint>> CrystalSpawnPoints;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Gimmick")
    TSubclassOf<ADeathWall> DeathWallClass;

    // 죽음의 벽 생성 위치 (필요 시 이것도 TargetPoint로 변경 권장, 현재는 기존 유지)
    UPROPERTY(EditAnywhere, Category = "Boss|Gimmick", meta = (MakeEditWidget = true))
    FVector WallSpawnLocation;

    // --- [Combat Config: 전투 수치] ---
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

    // 사망 처리
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

    // 애니메이션 노티파이: 투사체 발사
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Combat")
    void AnimNotify_SpawnSlash();

    // 애니메이션 노티파이: 근접 공격 판정
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Combat")
    void AnimNotify_CheckMeleeHit();

    // --- [Phase Logic] ---
protected:
    bool bPhase2Started = false;
    void EnterPhase2();

    // --- [Internal Logic] ---
private:
    // [Refactored] 고정 위치 스폰 함수
    void SpawnCrystals();
    void SpawnDeathWall();

    // 몽타주 재생 멀티캐스트
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayAttackMontage(UAnimMontage* MontageToPlay, FName SectionName = NAME_None);

    // QTE 관련 내부 함수
    void EndSpearQTE();
    void PerformWipeAttack();

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_SetQTEWidget(bool bVisible);

    TMap<APlayerController*, FPlayerQTEStatus> QTEProgressMap;
    FTimerHandle QTETimerHandle;

public:
    // 이 함수를 Behavior Tree Task에서 호출하거나 내부 로직에서 호출
    UFUNCTION(BlueprintCallable, Category = "Boss Pattern")
    void TrySpawnSpikeAtRandomPlayer();

protected:
    UPROPERTY(EditAnywhere, Category = "Boss Pattern")
    TSubclassOf<class ABossSpikeTrap> SpikeTrapClass;

    UPROPERTY(EditAnywhere, Category = "Boss Pattern")
    TObjectPtr<UAnimMontage> SpellMontage;

    // 몽타주 재생 (멀티캐스트)
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlaySpellMontage();
};