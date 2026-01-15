#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "TitanCharacter.generated.h"

class ATitanRock;
class AAIController;

UCLASS()
class PROJECT_BANG_SQUAD_API ATitanCharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
	ATitanCharacter();

protected:
	// =================================================================
	// [생명주기 및 오버라이드]
	// =================================================================
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnDeath() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// =================================================================
	// [입력 핸들러] (클라이언트에서 최초 호출)
	// =================================================================
	virtual void Attack() override;      // 평타
	virtual void JobAbility() override;  // 잡기/던지기
	virtual void Skill1() override;      // 바위 던지기
	virtual void Skill2() override;      // 돌진

public:
	// 몽타주 노티파이 등에서 호출될 수 있는 잡기 시도 함수
	UFUNCTION(BlueprintCallable)
	void ExecuteGrab();

protected:
	// =================================================================
	// [네트워크: 평타 (Attack)]
	// =================================================================
	UFUNCTION(Server, Reliable)
	void Server_Attack(FName SkillName);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Attack(FName SkillName);

	// =================================================================
	// [네트워크: 직업 스킬 (JobAbility - Grab/Throw)]
	// =================================================================
	UFUNCTION(Server, Reliable)
	void Server_TryGrab(AActor* TargetToGrab); // 잡기 요청

	UFUNCTION(Server, Reliable)
	void Server_ThrowTarget(); // 던지기 요청

	UFUNCTION()
	void OnRep_GrabbedActor(); // 잡힌 상태 동기화 (핵심)

	// =================================================================
	// [네트워크: 스킬 1 (Rock Throw)]
	// =================================================================
	UFUNCTION(Server, Reliable)
	void Server_Skill1();

	void ThrowRock(); // 타이머에 의해 서버에서 호출됨

	// =================================================================
	// [네트워크: 스킬 2 (Charge)]
	// =================================================================
	UFUNCTION(Server, Reliable)
	void Server_Skill2();

	UFUNCTION()
	void OnChargeOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnChargeHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	// =================================================================
	// [헬퍼 함수 및 변수]
	// =================================================================
	void ProcessSkill(FName SkillRowName, FName StartSectionName = NAME_None);
	void UpdateHoverHighlight();
	void SetHighlight(AActor* Target, bool bEnable);
	void SetHeldState(ACharacter* Target, bool bIsHeld);

	UFUNCTION()
	void RecoverCharacter(ACharacter* Victim);

	void StopCharge();
	void ResetCooldown();
	void ResetSkill1Cooldown();
	void ResetSkill2Cooldown();

private:
	// 공격 관련
	bool bIsNextAttackA = true;
	float AttackCooldownTime = 0.f;

	// 잡기 관련
	UPROPERTY(ReplicatedUsing = OnRep_GrabbedActor) // 변수 변경 시 OnRep 자동 호출
		AActor* GrabbedActor = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsGrabbing = false;

	UPROPERTY()
	AActor* HoveredActor = nullptr;

	FTimerHandle GrabTimerHandle;
	FTimerHandle CooldownTimerHandle;

	float ThrowForce = 3500.f;
	float GrabMaxDuration = 5.0f;
	float ThrowCooldownTime = 3.0f;

	// 스킬 1 (바위) 관련
	UPROPERTY(EditDefaultsOnly, Category = "Skill|Rock")
	TSubclassOf<ATitanRock> RockClass;

	UPROPERTY()
	ATitanRock* HeldRock = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Skill|Rock")
	float RockThrowForce = 2500.0f;

	FTimerHandle RockThrowTimerHandle;
	FTimerHandle Skill1CooldownTimerHandle;
	float Skill1CooldownTime = 3.0f;
	bool bIsSkill1Cooldown = false;

	// 스킬 2 (돌진) 관련
	FTimerHandle Skill2CooldownTimerHandle;
	FTimerHandle ChargeTimerHandle;
	float Skill2CooldownTime = 5.0f;
	bool bIsSkill2Cooldown = false;
	bool bIsCharging = false;

	UPROPERTY()
	TArray<AActor*> HitVictims;

	// 공통
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsCooldown = false; 

	float CurrentSkillDamage = 0.0f;
	float DefaultGroundFriction;
	float DefaultGravityScale;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data", meta = (AllowPrivateAccess = "true"))
	class UDataTable* SkillDataTable;
};