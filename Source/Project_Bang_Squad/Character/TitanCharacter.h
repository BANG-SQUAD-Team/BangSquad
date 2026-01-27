#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Components/ArrowComponent.h"
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
	// [입력 핸들러]
	// =================================================================
	virtual void Attack() override;      // 평타
	virtual void JobAbility() override;  // 잡기/던지기
	virtual void Skill1() override;      // 바위 던지기
	virtual void Skill2() override;      // 돌진
	
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Skill1();
	
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Skill2();

public:
	// 잡기 실행 함수 (몽타주 등에서 호출 가능)
	UFUNCTION(BlueprintCallable)
	void ExecuteGrab();

	UFUNCTION(BlueprintCallable, Category = "Titan|Skill")
	void ExecuteSpawnRock();

	UFUNCTION(BlueprintCallable, Category = "Titan|Skill")
	void ExecuteThrowRock();

protected:
	// =================================================================
	// [공격 판정 (Trace/Sweep) 변수]
	// =================================================================

	// 공격 판정 박스 크기
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	FVector HitBoxSize = FVector(80.0f, 80.0f, 80.0f);

	// 판정 지속 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float HitDuration = 0.25f;

	// 타이머 핸들
	FTimerHandle AttackHitTimerHandle; // 딜레이용
	FTimerHandle HitLoopTimerHandle;   // 반복 판정용

	// 궤적 계산용
	FVector LastHandLocation;

	UPROPERTY()
	TSet<AActor*> SwingDamagedActors; // 중복 타격 방지

	// 판정 함수들
	void StartMeleeTrace();
	void PerformMeleeTrace();
	void StopMeleeTrace();

	FName MyAttackSocket;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_FixMesh(ACharacter* Victim);

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
	void Server_TryGrab(AActor* TargetToGrab);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UArrowComponent* ThrowSpawnPoint;

	UFUNCTION(Server, Reliable)
	void Server_ThrowTarget(FVector ThrowStartLocation);

	void AutoThrowTimeout();

	UFUNCTION()
	void OnRep_GrabbedActor();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayJobMontage(FName SectionName);

	void PerformRadialImpact(FVector Origin, float Radius, float Damage, float RadialForce, AActor* IgnoreTarget = nullptr);

	// 던져진 대상이 어딘가 부딪혔을 때 호출될 함수 (델리게이트용)
	UFUNCTION()
	void OnThrownActorHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ForceThrowCleanup(AActor* TargetActor, FVector Velocity);
	
	// =================================================================
	// [네트워크: 스킬 1 (Rock Throw)]
	// =================================================================
	UFUNCTION(Server, Reliable)
	void Server_Skill1();

	void ThrowRock();

	UFUNCTION(Server, Reliable)
	void Server_SpawnRock();

	UFUNCTION(Server, Reliable)
	void Server_ThrowRock();

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
	UPROPERTY(ReplicatedUsing = OnRep_GrabbedActor)
	AActor* GrabbedActor = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsGrabbing = false;

	UPROPERTY()
	AActor* HoveredActor = nullptr;

	FTimerHandle GrabTimerHandle;
	FTimerHandle CooldownTimerHandle;

	float ThrowForce = 800.f;
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