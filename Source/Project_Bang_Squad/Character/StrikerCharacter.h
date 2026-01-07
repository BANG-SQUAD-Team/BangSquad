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
	virtual void Skill1() override;
	virtual void Skill2() override;
	virtual void JobAbility() override;

	UPROPERTY(EditDefaultsOnly, Category = "Data")
	class UDataTable* SkillDataTable;

private:
	void ProcessSkill(FName SkillRowName);

	/** 공중에 뜬 적을 찾아 반환하는 함수 */
	AActor* FindAirborneTarget();
};