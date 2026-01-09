#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TimerManager.h" // FTimerHandle 때문에 필요

#include "EnemyCharacterBase.generated.h"

class UAnimMontage;
class UHealthComponent;

UCLASS()
class PROJECT_BANG_SQUAD_API AEnemyCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	AEnemyCharacterBase();

protected:
	virtual void BeginPlay() override;

public:
	// ===== Hit React =====
	UFUNCTION(BlueprintCallable, Category = "HitReact")
	void ReceiveHitReact();

	UFUNCTION(BlueprintPure, Category = "HitReact")
	bool IsHitReacting() const { return bIsHitReacting; }

	// ===== Death =====
	UFUNCTION(BlueprintCallable, Category = "Death")
	void ReceiveDeath();

	UFUNCTION(BlueprintPure, Category = "Death")
	bool IsDead() const { return bIsDead; }

protected:
	// 파생(EnemyNormal 등)이 죽을 때 자기 타이머/상태 정리할 수 있는 훅
	virtual void OnDeathStarted();


	// ===== HealthComponent Auto Bind =====
	UFUNCTION()
	void HandleDeadFromHealth();

	UFUNCTION()
	void HandleHealthChangedFromHealth(float NewHealth, float InMaxHealth);

	// ===== Hit React Settings =====
	UPROPERTY(EditDefaultsOnly, Category = "HitReact")
	TObjectPtr<UAnimMontage> HitReactMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "HitReact", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float HitReactSpeedMultiplier = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "HitReact", meta = (ClampMin = "0.05", ClampMax = "5.0"))
	float HitReactMinDuration = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "HitReact")
	bool bIgnoreHitReactWhileActive = true;

	// ===== Death Settings =====
	UPROPERTY(EditDefaultsOnly, Category = "Death")
	TObjectPtr<UAnimMontage> DeathMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Death")
	bool bLoopDeathMontage = false;

	UPROPERTY(EditDefaultsOnly, Category = "Death", meta = (EditCondition = "bLoopDeathMontage"))
	FName DeathLoopSectionName = TEXT("Loop");

	UPROPERTY(EditDefaultsOnly, Category = "Death")
	bool bEnableRagdollOnDeath = true;

	UPROPERTY(EditDefaultsOnly, Category = "Death", meta = (ClampMin = "0.0", ClampMax = "10.0", EditCondition = "bEnableRagdollOnDeath"))
	float DeathToRagdollDelay = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Death")
	bool bDestroyAfterDeath = true;

	UPROPERTY(EditDefaultsOnly, Category = "Death", meta = (ClampMin = "0.0", ClampMax = "60.0", EditCondition = "bDestroyAfterDeath"))
	float DestroyDelay = 8.0f;

protected:
	void StartHitReact(float Duration);
	void EndHitReact();

	void StartDeath();
	void EnterRagdoll();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayHitReactMontage();
	void Multicast_PlayHitReactMontage_Implementation();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayDeathMontage();
	void Multicast_PlayDeathMontage_Implementation();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_EnterRagdoll();
	void Multicast_EnterRagdoll_Implementation();

private:
	bool bIsHitReacting = false;
	bool bIsDead = false;

	float DefaultMaxWalkSpeed = 0.f;

	FTimerHandle HitReactTimer;
	FTimerHandle DeathToRagdollTimer;
};
