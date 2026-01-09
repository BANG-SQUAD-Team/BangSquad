#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h"
#include "EnemyNormal.generated.h"

class UAnimMontage;

UCLASS()
class PROJECT_BANG_SQUAD_API AEnemyNormal : public AEnemyCharacterBase
{
	GENERATED_BODY()

public:
	AEnemyNormal();

protected:
	virtual void BeginPlay() override;
	virtual void OnDeathStarted() override;
	// ===== Chase =====
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Chase")
	float AcceptanceRadius = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Chase")
	float RepathInterval = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Chase")
	float StopChaseDistance = 2500.f;

	// ===== Attack =====
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Attack")
	float AttackRange = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Attack")
	float AttackCooldown = 1.2f;

	// "데미지" 자체가 없어서 안 맞던 문제의 핵심. 반드시 추가.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Attack")
	float AttackDamage = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Attack")
	TArray<TObjectPtr<UAnimMontage>> AttackMontages;

private:
	TWeakObjectPtr<APawn> TargetPawn;
	FTimerHandle RepathTimer;

	bool bIsAttacking = false;
	float LastAttackTime = -9999.f;

	FTimerHandle AttackEndTimer;

	// 한 번의 공격(몽타주 1회)에서 데미지 1번만 들어가게 잠금
	bool bDamageAppliedThisAttack = false;

	void AcquireTarget();
	void StartChase(APawn* NewTarget);
	void StopChase();
	void UpdateMoveTo();

	bool IsInAttackRange() const;

	// 서버에서 공격 시작 (멀티 정석)
	UFUNCTION(Server, Reliable)
	void Server_TryAttack();
	void Server_TryAttack_Implementation();

	// 모든 클라에서 "선택된 인덱스" 몽타주 재생
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayAttackMontage(int32 MontageIndex);
	void Multicast_PlayAttackMontage_Implementation(int32 MontageIndex);

	void EndAttack();

public:
	// AnimNotify에서 호출될 타격 처리 (서버만 데미지 적용)
	void AnimNotify_AttackHit();
};
