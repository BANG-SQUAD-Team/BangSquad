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
	virtual void JobAbility() override;

public:
	// 공격 시 이동 (몽타주 노티파이에서 호출됨)
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ApplyAttackForwardForce();

protected:
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

	// =================================================================
	// [네트워크: 스킬 1 (공중 콤보)]
	// =================================================================
	UFUNCTION(Server, Reliable)
	void Server_TrySkill1(AActor* TargetActor);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlaySkill1FX(AActor* Target);

	// =================================================================
	// [네트워크: 스킬 2 (내려찍기)]
	// =================================================================
	UFUNCTION(Server, Reliable)
	void Server_StartSkill2();

	UFUNCTION(Server, Reliable)
	void Server_Skill2Impact(); // 착지 시 데미지 처리

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlaySlamFX();

	// =================================================================
	// [네트워크: 직업 스킬 (범위 공격)]
	// =================================================================
	UFUNCTION(Server, Reliable)
	void Server_UseJobAbility();

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