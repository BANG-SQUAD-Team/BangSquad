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
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	virtual void JobAbility() override;
	virtual void Attack() override;
	virtual void Skill1() override;
	virtual void Skill2() override;

	void ProcessSkill(FName SkillRowName, FName StartSectionName = NAME_None);

	void UpdateHoverHighlight();
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsGrabbing = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsCooldown = false;

	UPROPERTY()
	AActor* GrabbedActor = nullptr;

	UPROPERTY()
	AActor* HoveredActor = nullptr;

	/** 타이탄 스킬용 데이터 테이블 (에디터에서 할당) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data", meta = (AllowPrivateAccess = "true"))
	class UDataTable* SkillDataTable;

	FTimerHandle GrabTimerHandle;
	FTimerHandle CooldownTimerHandle;

	float ThrowForce = 3500.f;
	float GrabMaxDuration = 5.0f;
	float ThrowCooldownTime = 3.0f;

	void TryGrab();
	void ThrowTarget();
	void ResetCooldown();
	void SetHighlight(AActor* Target, bool bEnable);

	UFUNCTION()
	void RecoverCharacter(ACharacter* Victim);

public:
	UFUNCTION(BlueprintCallable)
	void ExecuteGrab();
};