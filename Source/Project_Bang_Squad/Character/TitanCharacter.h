#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "TitanCharacter.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API ATitanCharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
	ATitanCharacter();

protected:
	virtual void Skill1() override;
	virtual void Skill2() override;
	virtual void JobAbility() override;

	/** 타이탄 전용 데이터 테이블 */
	UPROPERTY(EditDefaultsOnly, Category = "Data")
	class UDataTable* SkillDataTable;

private:
	/** 스킬 실행 공통 로직 */
	void ProcessSkill(FName SkillRowName);

	/* --- 직업 능력(잡기/던지기) 관련 변수 --- */
	UPROPERTY()
	AActor* GrabbedActor;

	bool bIsGrabbing = false;
	FTimerHandle GrabTimerHandle;

	void TryGrab();
	void ThrowTarget();
	void ReleaseGrab(); // 5초 시간 초과 시 자동 해제용
};