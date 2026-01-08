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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Chase")
	float AcceptanceRadius = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Chase")
	float RepathInterval = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Chase")
	float StopChaseDistance = 2500.f;

	// ===== Attack (멀티 고려) =====
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Attack")
	float AttackRange = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Attack")
	float AttackCooldown = 1.2f;

	// 공격 몽타주 3개(또는 그 이상) 넣는 배열
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Attack")
	TArray<TObjectPtr<UAnimMontage>> AttackMontages;

private:
	TWeakObjectPtr<APawn> TargetPawn;
	FTimerHandle RepathTimer;

	bool bIsAttacking = false;
	float LastAttackTime = -9999.f;

	FTimerHandle AttackEndTimer;

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
};
