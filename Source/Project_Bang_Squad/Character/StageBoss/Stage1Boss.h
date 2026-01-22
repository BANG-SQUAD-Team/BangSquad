// Source/Project_Bang_Squad/Character/StageBoss/Stage1Boss.h
#pragma once

#include "CoreMinimal.h"
#include "StageBossBase.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBossData.h" // 데이터 에셋
#include "Project_Bang_Squad/Core/BSGameInstance.h" // [필수] EJobType 사용을 위해 포함
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
    // [변경] 기존 단일 클래스 대신, 직업별로 다른 BP를 지정할 수 있는 Map 사용
    // 에디터 사용법: '+' 눌러서 Key(직업) - Value(BP) 쌍을 4개 등록하세요.
    UPROPERTY(EditDefaultsOnly, Category = "Boss|Gimmick")
    TMap<EJobType, TSubclassOf<AJobCrystal>> JobCrystalClasses;

    // 수정이 소환될 위치 (에디터 뷰포트에서 위젯으로 조정)
    UPROPERTY(EditAnywhere, Category = "Boss|Gimmick", meta = (MakeEditWidget = true))
    TArray<FVector> CrystalSpawnPoints;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Gimmick")
    TSubclassOf<ADeathWall> DeathWallClass;

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
};