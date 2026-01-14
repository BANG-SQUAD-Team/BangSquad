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

	// === 스킬 오버라이드 ===
	virtual void Attack() override;
	virtual void Skill1() override;
	virtual void Skill2() override;
	virtual void JobAbility() override;

	UPROPERTY(EditDefaultsOnly, Category = "Data")
	class UDataTable* SkillDataTable;

	// [수정] 스킬 쿨타임 관리용 변수들 (타임스탬프 방식)
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
	// 공통 스킬 데이터 처리
	void ProcessSkill(FName SkillRowName);

	// === [스킬 2: 공중 찍기] ===
	bool bIsSlamming = false;

	UFUNCTION(Server, Reliable)
	void Server_Skill2Impact();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlaySlamFX();

	// === [스킬 1: 야스오 궁] ===
	AActor* FindBestAirborneTarget();

	UFUNCTION(Server, Reliable)
	void Server_TrySkill1(AActor* TargetActor);

	void SuspendTarget(ACharacter* Target);

	UFUNCTION()
	void ReleaseTarget(ACharacter* Target);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlaySkill1FX(AActor* Target);

	// === [직업 능력: 전방 띄우기] ===
	UFUNCTION(Server, Reliable)
	void Server_UseJobAbility();
};