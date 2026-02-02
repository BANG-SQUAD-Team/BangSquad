#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "StrikerCharacter.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API AStrikerCharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
	AStrikerCharacter();

protected:
	// =================================================================
	// [생명주기 및 오버라이드]
	// =================================================================
	virtual void BeginPlay() override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void OnDeath() override;

	// =================================================================
	// [입력 핸들러]
	// =================================================================
	virtual void Attack() override;
	virtual void Skill1() override;
	virtual void Skill2() override;
	void Multicast_Skill2_Implementation();
	virtual void JobAbility() override;

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Skill2();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_JobAbility();
	
public:
	// 공격 시 이동 (몽타주 노티파이에서 호출됨)
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ApplyAttackForwardForce();

	void ApplyJobAbilityHit();

protected:
	// =================================================================
	// [공격 판정 (Trace/Sweep) 변수]
	// =================================================================

	// 공격 판정 박스 크기
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	FVector HitBoxSize = FVector(40.0f, 40.0f, 40.0f);

	// 판정 지속 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float HitDuration = 0.25f;

	// 타이머 핸들
	FTimerHandle AttackHitTimerHandle;
	FTimerHandle HitLoopTimerHandle;

	// 궤적 계산용
	FVector LastHandLocation;

	UPROPERTY()
	TSet<AActor*> SwingDamagedActors;

	// 판정 함수들
	void StartMeleeTrace();
	void PerformMeleeTrace();
	void StopMeleeTrace();

	FName MyAttackSocket;

	// =================================================================
	// [네트워크: 평타 (Attack)]
	// =================================================================
	UFUNCTION(Server, Reliable)
	void Server_Attack(FName SkillName);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Attack(FName SkillName);

	// 공격 이동 동기화
	UFUNCTION(Server, Reliable)
	void Server_ApplyAttackForwardForce();

	UPROPERTY()
	TArray<AActor*> HitVictims;

	float CurrentSkillDamage = 0.0f;

	// =================================================================
	// [네트워크: 스킬 1 (공중 콤보)]
	// =================================================================
	UFUNCTION(Server, Reliable)
	void Server_TrySkill1(AActor* TargetActor);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlaySkill1FX(AActor* Target);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Skill1")
	TSubclassOf<AActor> SlashActorClass;

	FTimerHandle SlashLoopTimerHandle;

	void SpawnRandomSlashFX();

	// =================================================================
	// [네트워크: 스킬 2 (내려찍기)]
	// =================================================================
	UFUNCTION(Server, Reliable)
	void Server_StartSkill2();

	UFUNCTION(Server, Reliable)
	void Server_Skill2Impact(); // 착지 시 데미지 처리

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlaySlamFX();

	FTimerHandle GroundCheckTimerHandle;

	void CheckGroundDistanceForSkill2();

	bool bHasTriggeredLandAnim = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|VFX")
	UMaterialInterface* Skill2DecalMaterial; // 데칼 머티리얼 (BP에서 할당)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|VFX")
	FVector Skill2DecalSize = FVector(200.0f, 200.0f, 200.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|VFX")
	float Skill2DecalLifeSpan = 2.0f; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|VFX")
	TSubclassOf<class UCameraShakeBase> Skill2CameraShakeClass;

	// =================================================================
	// [네트워크: 직업 스킬 (범위 공격)]
	// =================================================================
	UFUNCTION(Server, Reliable)
	void Server_UseJobAbility();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|VFX")
	TSubclassOf<AActor> JobAbilityEffectClass;

	// =================================================================
	// [헬퍼 함수]
	// =================================================================
	void ProcessSkill(FName SkillRowName);
	AActor* FindBestAirborneTarget();
	void SuspendTarget(ACharacter* Target);

	UFUNCTION()
	void ReleaseTarget(ACharacter* Target);

	void EndSkill1();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Data")
	class UDataTable* SkillDataTable;

	// 쿨타임 및 설정값
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	float JobAbilityCooldownTime = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	float Skill1ReadyTime = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	float Skill2ReadyTime = 0.f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float AttackForwardForce = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Skill1")
	float Skill1RequiredHeight = 150.0f;

private:
	bool bIsNextAttackA = true;
	bool bIsSlamming = false;

	UPROPERTY()
	ACharacter* CurrentComboTarget;

	FTimerHandle Skill1TimerHandle;
};