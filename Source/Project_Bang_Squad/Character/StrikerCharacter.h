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
	virtual void BeginPlay() override;

	// [중요] 착지 감지 (찍기 스킬 판정용)
	virtual void Landed(const FHitResult& Hit) override;

	// 평타
	virtual void Attack() override;

	// 스킬
	virtual void Skill1() override;
	virtual void Skill2() override; // 공중 찍기

	// 직업 능력 (야스오 궁)
	virtual void JobAbility() override;

	UPROPERTY(EditDefaultsOnly, Category = "Data")
	class UDataTable* SkillDataTable;

private:
	// 공통 스킬 처리
	void ProcessSkill(FName SkillRowName);

	/* === Skill 2 (공중 찍기) 관련 === */
	bool bIsSlamming = false;

	// 착지 시 주변 적 처리 (서버)
	UFUNCTION(Server, Reliable)
	void Server_Skill2Impact();

	// 착지 이펙트 (멀티캐스트)
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlaySlamFX();

	/* === 직업 능력 (야스오 궁) 관련 === */
	AActor* FindBestAirborneTarget();

	UFUNCTION(Server, Reliable)
	void Server_TryJobAbility(AActor* TargetActor);

	void SuspendTarget(ACharacter* Target);

	UFUNCTION()
	void ReleaseTarget(ACharacter* Target);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayJobAbilityFX(AActor* Target);
};