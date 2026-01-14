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

	// 부모 (BaseCharacter)의 OnDeath 상속
	virtual void OnDeath()override;
	
	// =============================================================
	// �Է� ���ε� �Լ� (Override)
	// =============================================================
	virtual void JobAbility() override; // ���/������
	virtual void Attack() override;     // ��Ÿ
	virtual void Skill1() override;     // �̱��� (����)
	virtual void Skill2() override;     // ���� (Charge)

	// ��ų ���� ó�� (��Ÿ�� ��� �� ���� ����)
	void ProcessSkill(FName SkillRowName, FName StartSectionName = NAME_None);

	// ��� ��� ���̶���Ʈ ����
	void UpdateHoverHighlight();

private:
	// =============================================================
	// [Job Ability] ��� & ������ ���� ����
	// =============================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsGrabbing = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsCooldown = false; // ��� ��Ÿ��

	UPROPERTY(Replicated) // ��Ƽ�÷��� ����ȭ
		AActor* GrabbedActor = nullptr;

	UPROPERTY()
	AActor* HoveredActor = nullptr;

	FTimerHandle GrabTimerHandle;
	FTimerHandle CooldownTimerHandle;

	float ThrowForce = 3500.f;
	float GrabMaxDuration = 5.0f;
	float ThrowCooldownTime = 3.0f;

	// ��� ���� ����
	void TryGrab();
	void ThrowTarget();
	void ResetCooldown();
	void SetHighlight(AActor* Target, bool bEnable);

	// ��Ҵ� Ǯ���� ĳ���� ���� ����
	UFUNCTION()
	void RecoverCharacter(ACharacter* Victim);

	// ������ �� AI/���� ����
	void SetHeldState(ACharacter* Target, bool bIsHeld);

public:
	// �ִϸ��̼� ��Ƽ���̿��� ȣ���� �Լ� (���� ���� Ÿ�̹�)
	UFUNCTION(BlueprintCallable)
	void ExecuteGrab();

protected:
	// ���� ��ų ���� ���� ��û
	UFUNCTION(Server, Reliable)
	void Server_Skill2();

	// ���� �浹 ���� (Overlap)
	UFUNCTION()
	void OnChargeOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// ���� �� �浹 ���� (Hit)
	UFUNCTION()
	void OnChargeHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void StopCharge();
	void ResetSkill2Cooldown();

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsCharging = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsSkill2Cooldown = false;

	// ���� �� �ߺ� �ǰ� ������
	UPROPERTY()
	TArray<AActor*> HitVictims;

	FTimerHandle Skill2CooldownTimerHandle;
	FTimerHandle ChargeTimerHandle;

	// �뷱�� ������
	float Skill2CooldownTime = 5.0f; // ��Ÿ��
	float CurrentSkillDamage = 0.0f; // ���������̺��� ������

	// ���� ������ ��� ����
	float DefaultGroundFriction;
	float DefaultGravityScale;

	/** Ÿ��ź ��ų ������ ���̺� */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data", meta = (AllowPrivateAccess = "true"))
	class UDataTable* SkillDataTable;
};