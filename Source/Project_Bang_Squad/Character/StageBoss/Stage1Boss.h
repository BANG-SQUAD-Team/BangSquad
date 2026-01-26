// Source/Project_Bang_Squad/Character/StageBoss/Stage1Boss.h
#pragma once

#include "CoreMinimal.h"
#include "StageBossBase.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBossData.h"
#include "Project_Bang_Squad/Core/BSGameInstance.h"
#include "Engine/TargetPoint.h"
#include "AITypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "Stage1Boss.generated.h"

class AJobCrystal;
class ASlashProjectile;
class UAnimMontage;
class ADeathWall;

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

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Data")
    TObjectPtr<UEnemyBossData> BossData;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Gimmick")
    TMap<EJobType, TSubclassOf<AJobCrystal>> JobCrystalClasses;

    UPROPERTY(EditInstanceOnly, Category = "Boss|Gimmick")
    TArray<TObjectPtr<ATargetPoint>> CrystalSpawnPoints;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Gimmick")
    TSubclassOf<ADeathWall> DeathWallClass;

    UPROPERTY(EditInstanceOnly, Category = "Boss|Gimmick")
    TObjectPtr<ATargetPoint> DeathWallCastLocation;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Gimmick")
    float DeathWallPatternDuration = 10.0f;

    UPROPERTY(EditAnywhere, Category = "Boss|Gimmick", meta = (MakeEditWidget = true))
    FVector WallSpawnLocation;

    UPROPERTY(EditAnywhere, Category = "Boss|Combat")
    float MeleeAttackRadius = 400.0f;

    UPROPERTY(EditAnywhere, Category = "Boss|Combat")
    float MeleeDamageAmount = 30.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|QTE")
    float QTEDuration = 3.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|QTE")
    int32 QTEGoalCount = 10;

protected:
    virtual void OnPhaseChanged(EBossPhase NewPhase) override;
    virtual void OnGimmickResolved(int32 GimmickID) override;
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
    virtual void OnDeathStarted() override;

public:
    UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
    void DoAttack_Slash();

    UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
    void DoAttack_Swing();

    UFUNCTION(BlueprintCallable, Category = "Boss|QTE")
    void StartSpearQTE();

    UFUNCTION(Server, Reliable, BlueprintCallable)
    void Server_SubmitQTEInput(APlayerController* PlayerController);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Combat")
    void AnimNotify_SpawnSlash();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Combat")
    void AnimNotify_CheckMeleeHit();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Combat")
    void AnimNotify_ActivateDeathWall();

    UFUNCTION(BlueprintCallable, Category = "Boss Pattern")
    void StartSpikePattern();

    UFUNCTION(BlueprintCallable, Category = "Boss Pattern")
    void ExecuteSpikeSpell();

protected:
    bool bPhase2Started = false;
    void EnterPhase2();

private:
    void SpawnCrystals();
    void SpawnDeathWall();

    // 몽타주 재생 멀티캐스트 (Reliable로 변경하여 패킷 유실 방지)
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayAttackMontage(UAnimMontage* MontageToPlay, FName SectionName = NAME_None);

    // 몽타주 종료 콜백
    UFUNCTION()
    void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    void EndSpearQTE();
    void PerformWipeAttack();

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_SetQTEWidget(bool bVisible);

    TMap<APlayerController*, FPlayerQTEStatus> QTEProgressMap;
    FTimerHandle QTETimerHandle;

protected:
    bool bHasTriggeredDeathWall = false;
    bool bIsDeathWallSequenceActive = false;
    FTimerHandle DeathWallTimerHandle;
    FTimerHandle FailSafeTimerHandle;

    UFUNCTION()
    void OnHealthChanged(float CurrentHealth, float MaxHealth);

    void StartDeathWallSequence();

    UFUNCTION()
    void OnArrivedAtCastLocation(FAIRequestID RequestID, EPathFollowingResult::Type Result);

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayDeathWallMontage();

    void FinishDeathWallPattern();

public:
    UFUNCTION(BlueprintCallable, Category = "Boss Pattern")
    void TrySpawnSpikeAtRandomPlayer();

protected:
    UPROPERTY(EditAnywhere, Category = "Boss Pattern")
    TSubclassOf<class ABossSpikeTrap> SpikeTrapClass;

    UPROPERTY(EditAnywhere, Category = "Boss Pattern")
    TObjectPtr<UAnimMontage> SpellMontage;

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlaySpellMontage();
};