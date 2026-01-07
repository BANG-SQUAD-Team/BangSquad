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

	/** 직업 능력 및 스킬 */
	virtual void JobAbility() override;
	virtual void Skill1() override;
	virtual void Skill2() override;

	/** 실시간 하이라이트 업데이트 */
	void UpdateHoverHighlight();

private:
	// --- 상태 변수 ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsGrabbing = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsCooldown = false;

	UPROPERTY()
	AActor* GrabbedActor = nullptr;

	UPROPERTY()
	AActor* HoveredActor = nullptr;

	// --- 타이머 핸들 ---
	FTimerHandle GrabTimerHandle;
	FTimerHandle CooldownTimerHandle;

	// --- 설정값 ---
	float ThrowForce = 3500.f;
	float GrabMaxDuration = 5.0f;
	float ThrowCooldownTime = 3.0f;

	// --- 핵심 로직 함수 ---
	void TryGrab();
	void ThrowTarget();
	void ResetCooldown();
	void SetHighlight(AActor* Target, bool bEnable);

	UFUNCTION()
	void RecoverCharacter(ACharacter* Victim);

};