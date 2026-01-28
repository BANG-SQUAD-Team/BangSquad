// Source/Project_Bang_Squad/Character/StageBoss/Stage1Boss.h

#pragma once

#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "CoreMinimal.h"
#include "StageBossBase.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBossData.h"
#include "Project_Bang_Squad/Core/BSGameInstance.h"
#include "Engine/TargetPoint.h"
#include "AITypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "Project_Bang_Squad/BossPattern/Boss1_Rampart.h"
#include "AQTEObject.h" // [필수] QTE 오브젝트 헤더
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

	// [오버라이드] 데미지 처리 (기믹 발동 및 페이즈 전환 체크)
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	// [오버라이드] 페이즈 변경 시 호출
	virtual void OnPhaseChanged(EBossPhase NewPhase) override;

	// [오버라이드] 죽음 처리
	virtual void OnDeathStarted() override;

	// [오버라이드] 기믹 해결(수정 파괴 등) 시 호출
	virtual void OnGimmickResolved(int32 GimmickID) override;

protected:
	// --- [Data Asset Config] ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Data")
	TObjectPtr<UEnemyBossData> BossData;

	// 체력 변경 콜백
	UFUNCTION()
	void OnHealthChanged(float CurrentHealth, float MaxHealth);


	// ==============================================================================
	// [1] QTE 시스템 (Spear of Destiny) - GameMode 연동형
	// ==============================================================================
public:
	// [명령 수신] GameMode가 "QTE 비주얼 시작해"라고 호출
	void PlayQTEVisuals(float Duration);

	// [명령 수신] GameMode가 "QTE 결과(성공/실패)"를 통보
	void HandleQTEResult(bool bSuccess);

protected:
	// QTE용 오브젝트 BP (창/운석)
	UPROPERTY(EditAnywhere, Category = "Boss|Gimmick")
	TSubclassOf<class AQTEObject> QTEObjectClass;

	// 현재 생성된 QTE 오브젝트 참조
	UPROPERTY()
	TObjectPtr<class AQTEObject> ActiveQTEObject;

	// 기믹 발동 체력 비율 (예: 0.5 = 50%)
	UPROPERTY(EditAnywhere, Category = "Boss|Gimmick")
	float GimmickThresholdRatio = 0.5f;

	// 중복 발동 방지 플래그
	bool bHasTriggeredGimmick = false;


	// ==============================================================================
	// [2] 일반 공격 설정 (Melee Combat)
	// ==============================================================================
public:
	UPROPERTY(EditAnywhere, Category = "Boss|Combat")
	float MeleeDamageAmount = 30.0f;

	// 근접 공격 판정 박스
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Boss|Combat")
	TObjectPtr<UBoxComponent> MeleeCollisionBox;

	// [노티파이] 칼을 높게 든 지점 (잠시 멈춤)
	UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
	void AnimNotify_StartMeleeHold();

	// [노티파이] 타격 지점 (데미지 판정)
	UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
	void AnimNotify_ExecuteMeleeHit();

	// AI 명령용 공격 함수
	UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
	void DoAttack_Slash();

	UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
	void DoAttack_Swing();

protected:
	FTimerHandle MeleeAttackTimerHandle;

	void ReleaseMeleeAttackHold();

	// 애니메이션 속도 조절 (멀티캐스트)
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetAttackPlayRate(float NewRate);

	// 몽타주 재생 헬퍼
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayAttackMontage(UAnimMontage* MontageToPlay, FName SectionName = NAME_None);

	UFUNCTION()
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);


	// ==============================================================================
	// [3] 기믹: 죽음의 성벽 (Death Wall & Rampart)
	// ==============================================================================
public:
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Gimmick")
	TSubclassOf<ADeathWall> DeathWallClass;

	UPROPERTY(EditInstanceOnly, Category = "Boss|Gimmick")
	TObjectPtr<ATargetPoint> DeathWallCastLocation;

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

	// [노티파이] 벽 소환 시점
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Combat")
	void AnimNotify_ActivateDeathWall();

	// 이동 완료 콜백 (성벽 패턴 위치 이동용)
	UFUNCTION()
	void OnArrivedAtCastLocation(FAIRequestID RequestID, EPathFollowingResult::Type Result);


	// ==============================================================================
	// [4] 기믹: 직업 수정 (Job Crystals) & 페이즈 관리
	// ==============================================================================
public:
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Gimmick")
	TMap<EJobType, TSubclassOf<AJobCrystal>> JobCrystalClasses;

	UPROPERTY(EditInstanceOnly, Category = "Boss|Gimmick")
	TArray<TObjectPtr<ATargetPoint>> CrystalSpawnPoints;

protected:
	bool bPhase2Started = false;

	// 2페이즈 진입 (QTE 성공 후 호출)
	void EnterPhase2();
	void SpawnCrystals();

	// 전멸기 (QTE 실패 시 호출)
	void PerformWipeAttack();


	// ==============================================================================
	// [5] 패턴: 스파이크(가시) & 기타 마법
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
};