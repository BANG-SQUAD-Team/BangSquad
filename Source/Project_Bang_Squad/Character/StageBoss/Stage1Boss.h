// Source/Project_Bang_Squad/Character/StageBoss/Stage1Boss.h

#pragma once

#include "CoreMinimal.h"
#include "StageBossBase.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBossData.h"
#include "Project_Bang_Squad/Core/BSGameInstance.h"
#include "Engine/TargetPoint.h"
#include "AITypes.h"
#include "Navigation/PathFollowingComponent.h"
// [��� ����]
#include "Project_Bang_Squad/BossPattern/Boss1_Rampart.h" 
#include "AQTEObject.h" 
#include "Stage1Boss.generated.h"

// ���� ����
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
	
	// 모든 클라이언트의 화면에 자막 UI를 띄우도록 명령하는 함수
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = "BS|UI")
	void Multicast_ShowBossSubtitle(const FText& Message, float Duration = 3.0f);

	// 블루프린트에서 위젯 클래스를 할당받기 위한 변수
	UPROPERTY(EditDefaultsOnly, Category = "BS|UI")
	TSubclassOf<class UUserWidget> BossSubtitleWidgetClass;
	
	// 보스 체력바 관련 UI 변수 및 함수
	UPROPERTY(EditDefaultsOnly, Category = "BS|UI")
	TSubclassOf<class UUserWidget> BossHPWidgetClass;
	
	// 생성된 체력바 위젯을 기억해둘 포인터
	UPROPERTY()
	class UUserWidget* ActiveBossHPWidget;
	
	// 보스 등장 시 체력바 띄우기
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ShowBossHP(float MaxHP);
	
	// 데미지를 입을 때마다 보스 체력바 갱신
	UFUNCTION(NetMulticast,Reliable)
	void Multicast_UpdateBossHP(float CurrentHP, float MaxHP);
	
	// 보스 사망 시 체력바 지우기
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_HideBossHP();
protected:
	virtual void BeginPlay() override;

	// [Server-Only] ������ ��� �� ��� Ʈ���� üũ (���� �ʼ�)
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	// ������ ���� ���� �� ���� (Replicated)
	virtual void OnPhaseChanged(EBossPhase NewPhase) override;

	virtual void OnDeathStarted() override;
	virtual void OnGimmickResolved(int32 GimmickID) override;

	// ���� ����
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	// ���� ������ ���� (�ִϸ��̼�, ���� ��)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Data")
	TObjectPtr<UEnemyBossData> BossData;

	// ü�� ���� ��������Ʈ (���� ����)
	UFUNCTION()
	void OnHealthChanged(float CurrentHealth, float MaxHealth);


	// ==============================================================================
	// [1] ��� �ߵ� �÷��� (100% / 50% / 0% QTE)
	// ==============================================================================
protected:
	// 1. ���� ��(100%) ũ����Ż ��� �ߵ� ����
	bool bHasTriggeredCrystal_100 = false;

	// 2. �߰� �ܰ�(50%) ũ����Ż ��� �ߵ� ����
	bool bHasTriggeredCrystal_50 = false;

	// 3. QTE(0%) �ߵ� ����
	UPROPERTY(Replicated)
	bool bHasTriggeredQTE_10 = false;


	// ==============================================================================
	// [2] QTE �ý��� (�ǴϽ� ����)
	// ==============================================================================
public:
	// GameMode ���� ����: ���־� ���� ����
	void PlayQTEVisuals(float Duration);

	// GameMode ���� ����: ��� ó��
	void HandleQTEResult(bool bSuccess);

protected:
	// QTE ���� (â/�) Ŭ����
	UPROPERTY(EditAnywhere, Category = "Boss|Gimmick")
	TSubclassOf<class AQTEObject> QTEObjectClass;

	// ������ QTE ������Ʈ ����
	UPROPERTY()
	TObjectPtr<class AQTEObject> ActiveQTEObject;

	// QTE ���� �� �ִϸ��̼� ���� ó��
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_FreezeAnimation(bool bFreeze);


	// ==============================================================================
	// [3] �Ϲ� ���� (Combat)
	// ==============================================================================
public:
	UPROPERTY(EditAnywhere, Category = "Boss|Combat")
	float MeleeDamageAmount = 30.0f;

	// Į Ÿ�� ���� �ڽ�
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
	// [4] Ư�� ��� (Death Wall & Job Crystals)
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
	// [5] ������ ���� �� ��Ÿ ��ų
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

public:
	UPROPERTY(EditInstanceOnly, Category = "BS|Boss")
	TObjectPtr<class AMapPortal> TargetPortal;
};