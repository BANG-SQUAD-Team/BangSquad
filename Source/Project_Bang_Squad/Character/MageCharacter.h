#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "MageCharacter.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API AMageCharacter : public ABaseCharacter
{
    GENERATED_BODY()

public:
    AMageCharacter();

protected:
    /** 부모의 기능을 마법사 전용으로 재정의 */
    virtual void Skill1() override;
    virtual void Skill2() override;
    virtual void JobAbility() override;

    /** 마법사 스킬용 데이터 테이블 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data")
    class UDataTable* SkillDataTable;

private:
    /** 데이터 테이블에서 행을 찾아 로그를 찍거나 기능을 실행하는 함수 */
    void ProcessSkill(FName SkillRowName);

};