#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Misc/Optional.h"
#include "Engine/TextureDefines.h"
#include "StrikerCharacter.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API AStrikerCharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
	AStrikerCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void OnDeath() override;

	// === ��ų �������̵� ===
	virtual void Attack() override;
	virtual void Skill1() override;
	virtual void Skill2() override;
	virtual void JobAbility() override;

	UPROPERTY(EditDefaultsOnly, Category = "Data")
	class UDataTable* SkillDataTable;

	// [����] ��ų ��Ÿ�� ������ ������ (Ÿ�ӽ����� ���)
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
	// ���� ��ų ������ ó��
	void ProcessSkill(FName SkillRowName);

	// === [��ų 2: ���� ���] ===
	bool bIsSlamming = false;

	UFUNCTION(Server, Reliable)
	void Server_Skill2Impact();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlaySlamFX();

	// === [��ų 1: �߽��� ��] ===
	AActor* FindBestAirborneTarget();

	UFUNCTION(Server, Reliable)
	void Server_TrySkill1(AActor* TargetActor);

	void SuspendTarget(ACharacter* Target);

	UFUNCTION()
	void ReleaseTarget(ACharacter* Target);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlaySkill1FX(AActor* Target);

	// === [���� �ɷ�: ���� ����] ===
	UFUNCTION(Server, Reliable)
	void Server_UseJobAbility();
	
	// Skill1 상태 관리용
	void EndSkill1();
	
	// 현재 공중 콤보로 잡아둔 타겟 (죽을 때 놔주기 위해서 기억)
	UPROPERTY()
	ACharacter* CurrentComboTarget;
	
	// 콤보 끝나는 타이머 핸들
	FTimerHandle Skill1TimerHandle;
};