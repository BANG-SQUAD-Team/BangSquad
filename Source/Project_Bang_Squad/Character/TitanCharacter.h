#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "TitanCharacter.generated.h"

class AAIController;

UCLASS()
class PROJECT_BANG_SQUAD_API ATitanCharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
	ATitanCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void OnDeath() override;

	// =============================================================
	// 입력 바인딩 함수 (Override)
	// =============================================================
	virtual void JobAbility() override; // 잡기/던지기
	virtual void Attack() override;     // 평타
	virtual void Skill1() override;     // 내려찍기 (지진)
	virtual void Skill2() override;     // 돌진 (Charge)

	// 스킬 몽타주 재생 처리
	void ProcessSkill(FName SkillRowName, FName StartSectionName = NAME_None);

	// 마우스 오버 하이라이트
	void UpdateHoverHighlight();

private:
	// =============================================================
	// [Job Ability] 잡기 & 던지기
	// =============================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsGrabbing = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsCooldown = false; // 직업 스킬 쿨타임 체크용

	UPROPERTY(Replicated)
	AActor* GrabbedActor = nullptr;

	UPROPERTY()
	AActor* HoveredActor = nullptr;

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

	void SetHeldState(ACharacter* Target, bool bIsHeld);

public:
	// 애니메이션 노티파이에서 호출될 함수
	UFUNCTION(BlueprintCallable)
	void ExecuteGrab();

protected:
	// [Skill 2] 돌진
	UFUNCTION(Server, Reliable)
	void Server_Skill2();

	UFUNCTION()
	void OnChargeOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnChargeHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void StopCharge();
	void ResetSkill2Cooldown();

	// [Skill 1] 지진 (이 부분이 없어서 에러가 났던 것임)
	void ResetSkill1Cooldown();

private:
	// 돌진 관련
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsCharging = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsSkill2Cooldown = false;

	// [Skill 1] 쿨타임 관련 (이 변수들이 없어서 에러가 났던 것임)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsSkill1Cooldown = false;

	UPROPERTY()
	TArray<AActor*> HitVictims;

	FTimerHandle Skill2CooldownTimerHandle;
	FTimerHandle ChargeTimerHandle;
	FTimerHandle Skill1CooldownTimerHandle; // 타이머 핸들

	float Skill2CooldownTime = 5.0f;
	float Skill1CooldownTime = 3.0f; // 기본 쿨타임

	float CurrentSkillDamage = 0.0f;
	float DefaultGroundFriction;
	float DefaultGravityScale;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data", meta = (AllowPrivateAccess = "true"))
	class UDataTable* SkillDataTable;
};