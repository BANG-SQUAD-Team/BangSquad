// Source/Project_Bang_Squad/Character/StageBoss/Stage2Boss.h

#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/StageBoss/StageBossBase.h"
#include "Stage2Boss.generated.h"

// [전방 선언] BossData 클래스를 알리기 위함
class UEnemyBossData;

/**
 * Stage 2 보스: 거미 (Spider)
 */
UCLASS()
class PROJECT_BANG_SQUAD_API AStage2Boss : public AStageBossBase
{
	GENERATED_BODY()

public:
	AStage2Boss();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

public:
	// --- [DataAsset] ---
	/** 보스 데이터 에셋 (이 변수가 있어야 cpp에서 BossData를 쓸 수 있습니다) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Config")
	TObjectPtr<UEnemyBossData> BossData;

	// --- [1. 기본 공격] ---
	UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
	void PerformBasicAttack(int32 AttackIndex);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Combat")
	void ExecuteAttackTrace(FName SocketName, float AttackRadius = 60.0f);

	// --- [2. 백스텝] ---
	UFUNCTION(BlueprintCallable, Category = "Boss|Movement")
	void PerformBackstep();

	// --- [3. 기믹 A: 거미줄 발사] ---
	/** [핵심 수정] 이 선언이 없어서 에러가 발생했습니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Pattern")
	void PerformWebShot(AActor* TargetActor);

	// --- [4. 기믹 B: QTE] ---
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Pattern")
	void StartQTEPattern(AActor* TargetPlayer);

	UFUNCTION(Server, Reliable)
	void ServerRPC_QTEResult(APlayerController* Player, bool bSuccess);

	// --- [5. 전멸기] ---
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Pattern")
	void StartSplitPattern();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Pattern")
	void CheckSplitResult();

	// --- [6. 소환] ---
	void CheckSpawnPhase(float NewHP);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Pattern")
	void SpawnMinions();

	UFUNCTION()
	void OnMinionDestroyed(AActor* DestroyedActor);

protected:
	// --- [상태 변수] ---
	/** [Visual] 쉴드 이펙트 활성화 여부 */
	UPROPERTY(ReplicatedUsing = OnRep_ShieldActive)
	bool bShieldActive;

	UFUNCTION()
	void OnRep_ShieldActive();

	bool bHasTriggeredLastStand;
	int32 LivingMinionCount;
	FTimerHandle TimerHandle_QTEFail;

	// --- [Network Visual] ---
	/** [핵심 수정] 이 선언도 없어서 에러가 발생했습니다. */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayMontage(UAnimMontage* MontageToPlay);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ShowWipeEffect(bool bSuccess);
};