// Source/Project_Bang_Squad/Character/StageBoss/Stage1Boss.h
#pragma once

#include "CoreMinimal.h"
#include "StageBossBase.h"
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

    // --- [Gimmick Config: 수정 기믹] ---
public:
    UPROPERTY(EditDefaultsOnly, Category = "Boss|Gimmick")
    TSubclassOf<AJobCrystal> CrystalClass;

    UPROPERTY(EditAnywhere, Category = "Boss|Gimmick", meta = (MakeEditWidget = true))
    TArray<FVector> CrystalSpawnPoints;

    // --- [Combat Config: 전투 패턴] ---
    UPROPERTY(EditDefaultsOnly, Category = "Boss|Combat")
    TSubclassOf<ASlashProjectile> SlashProjectileClass;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Combat")
    UAnimMontage* AttackMontage;

    UPROPERTY(EditAnywhere, Category = "Boss|Combat")
    float MeleeAttackRadius = 400.0f;

    UPROPERTY(EditAnywhere, Category = "Boss|Combat")
    float MeleeDamageAmount = 30.0f;

    // --- [QTE Config: 창 던지기] ---
    UPROPERTY(EditDefaultsOnly, Category = "Boss|QTE")
    float QTEDuration = 3.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|QTE")
    int32 QTEGoalCount = 10;

    // --- [Gimmick Config: 죽음의 벽] ---
    UPROPERTY(EditDefaultsOnly, Category = "Boss|Gimmick")
    TSubclassOf<ADeathWall> DeathWallClass;

    UPROPERTY(EditAnywhere, Category = "Boss|Gimmick", meta = (MakeEditWidget = true))
    FVector WallSpawnLocation;

protected:
    // --- [Override Functions] ---
    virtual void OnPhaseChanged(EBossPhase NewPhase) override;
    virtual void OnGimmickResolved(int32 GimmickID) override;

    // 데미지 체크 (페이즈 2 진입용)
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

    // 사망 처리 (승리 로직) - 부모 클래스의 가상 함수 오버라이드
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

    // --- [AnimNotify Hooks] ---
protected:
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Combat")
    void AnimNotify_SpawnSlash();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Combat")
    void AnimNotify_CheckMeleeHit();

    // --- [Phase Logic] ---
protected:
    // 페이즈 2가 이미 시작되었는지 체크
    bool bPhase2Started = false;

    // 페이즈 2 진입 함수
    void EnterPhase2();

    // --- [Internal Logic] ---
private:
    void SpawnCrystals();
    void SpawnDeathWall();

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayAttackMontage(FName SectionName);

    // QTE 관련 내부 함수
    void EndSpearQTE();
    void PerformWipeAttack();

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_SetQTEWidget(bool bVisible);

    // QTE 상태 저장소
    TMap<APlayerController*, FPlayerQTEStatus> QTEProgressMap;
    FTimerHandle QTETimerHandle;
};