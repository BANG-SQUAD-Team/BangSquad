// Source/Project_Bang_Squad/Character/StageBoss/Stage1Boss.h

#pragma once

#include "CoreMinimal.h"
#include "StageBossBase.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBossData.h"
#include "Project_Bang_Squad/Core/BSGameInstance.h"
#include "Engine/TargetPoint.h"
#include "AITypes.h"
#include "Navigation/PathFollowingComponent.h"
// [경로 수정]
#include "Project_Bang_Squad/BossPattern/Boss1_Rampart.h" 
#include "AQTEObject.h" 
#include "Stage1Boss.generated.h"

// 전방 선언
class AJobCrystal;
class ASlashProjectile;
class UAnimMontage;
class ADeathWall;
class ABossSpikeTrap;
class UBoxComponent;
class UHealthComponent;

/**
 * Stage 1 Boss: Rampart Gimmick, Death Wall, Spear QTE, Job Crystal System
 */
UCLASS()
class PROJECT_BANG_SQUAD_API AStage1Boss : public AStageBossBase
{
	GENERATED_BODY()

public:
	AStage1Boss();

protected:
	virtual void BeginPlay() override;

	// [Server-Only] 데미지 계산 및 기믹 트리거 체크 (권한 필수)
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	// 페이즈 상태 변경 시 로직 (Replicated)
	virtual void OnPhaseChanged(EBossPhase NewPhase) override;

	virtual void OnDeathStarted() override;
	virtual void OnGimmickResolved(int32 GimmickID) override;

	// 변수 복제
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	// 보스 데이터 에셋 (애니메이션, 스탯 등)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Data")
	TObjectPtr<UEnemyBossData> BossData;

	// 체력 변경 델리게이트 (서버 전용)
	UFUNCTION()
	void OnHealthChanged(float CurrentHealth, float MaxHealth);


	// ==============================================================================
	// [1] 기믹 발동 플래그 (100% / 50% / 0% QTE)
	// ==============================================================================
protected:
	// 1. 조우 시(100%) 크리스탈 기믹 발동 여부
	bool bHasTriggeredCrystal_100 = false;

	// 2. 중간 단계(50%) 크리스탈 기믹 발동 여부
	bool bHasTriggeredCrystal_50 = false;

	// 3. QTE(0%) 발동 여부
	UPROPERTY(Replicated)
	bool bHasTriggeredQTE_10 = false;


	// ==============================================================================
	// [2] QTE 시스템 (피니시 연출)
	// ==============================================================================
public:
	// GameMode 명령 수신: 비주얼 연출 시작
	void PlayQTEVisuals(float Duration);

	// GameMode 명령 수신: 결과 처리
	void HandleQTEResult(bool bSuccess);

protected:
	// QTE 액터 (창/운석) 클래스
	UPROPERTY(EditAnywhere, Category = "Boss|Gimmick")
	TSubclassOf<class AQTEObject> QTEObjectClass;

	// 생성된 QTE 오브젝트 참조
	UPROPERTY()
	TObjectPtr<class AQTEObject> ActiveQTEObject;

	// QTE 진입 시 애니메이션 멈춤 처리
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_FreezeAnimation(bool bFreeze);


	// ==============================================================================
	// [3] 일반 공격 (Combat)
	// ==============================================================================
public:
	UPROPERTY(EditAnywhere, Category = "Boss|Combat")
	float MeleeDamageAmount = 30.0f;

	// 칼 타격 판정 박스
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Boss|Combat")
	TObjectPtr<UBoxComponent> MeleeCollisionBox;

	UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
	void AnimNotify_StartMeleeHold();

	UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
	void AnimNotify_ExecuteMeleeHit();

	UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
	void DoAttack_Slash();

	UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
	void DoAttack_Swing();

protected:
	FTimerHandle MeleeAttackTimerHandle;

	void ReleaseMeleeAttackHold();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetAttackPlayRate(float NewRate);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayAttackMontage(UAnimMontage* MontageToPlay, FName SectionName = NAME_None);

	UFUNCTION()
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);


	// ==============================================================================
	// [4] 특수 기믹 (Death Wall & Job Crystals)
	// ==============================================================================
public:
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Gimmick")
	TSubclassOf<ADeathWall> DeathWallClass;

	UPROPERTY(EditInstanceOnly, Category = "Boss|Gimmick")
	TObjectPtr<ATargetPoint> DeathWallCastLocation;

	UPROPERTY(EditDefaultsOnly, Category = "Boss|Gimmick")
	TMap<EJobType, TSubclassOf<AJobCrystal>> JobCrystalClasses;

	UPROPERTY(EditInstanceOnly, Category = "Boss|Gimmick")
	TArray<TObjectPtr<ATargetPoint>> CrystalSpawnPoints;

protected:
	bool bHasTriggeredDeathWall = false;
	bool bIsDeathWallSequenceActive = false;

	FTimerHandle DeathWallTimerHandle;
	FTimerHandle RampartTimerHandle;

	void StartDeathWallSequence();
	void SpawnDeathWall();
	void FinishDeathWallPattern();

	void ControlRamparts(bool bSink);
	void RestoreRamparts();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayDeathWallMontage();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Combat")
	void AnimNotify_ActivateDeathWall();

	UFUNCTION()
	void OnArrivedAtCastLocation(FAIRequestID RequestID, EPathFollowingResult::Type Result);


	// ==============================================================================
	// [5] 페이즈 관리 및 기타 스킬
	// ==============================================================================
protected:
	bool bPhase2Started = false;

	void EnterPhase2();
	void SpawnCrystals();
	void PerformWipeAttack();

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
};