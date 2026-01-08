#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyCharacterBase.generated.h"

class UAnimMontage;

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

	// 데스 몽타주를 루프로 돌리고 싶으면 true
	UPROPERTY(EditDefaultsOnly, Category = "Death")
	bool bLoopDeathMontage = false;

	// 루프 섹션 이름(몽타주에 섹션을 만들었을 때만 의미 있음)
	UPROPERTY(EditDefaultsOnly, Category = "Death", meta = (EditCondition = "bLoopDeathMontage"))
	FName DeathLoopSectionName = TEXT("Loop");

	// 몽타주 재생 후 래그돌로 전환할지
	UPROPERTY(EditDefaultsOnly, Category = "Death")
	bool bEnableRagdollOnDeath = true;

	// (몽타주 시작 후) 몇 초 뒤 래그돌로 전환할지
	UPROPERTY(EditDefaultsOnly, Category = "Death", meta = (ClampMin = "0.0", ClampMax = "10.0", EditCondition = "bEnableRagdollOnDeath"))
	float DeathToRagdollDelay = 0.25f;

	// 죽은 뒤 일정 시간 후 파괴(시체 정리)
	UPROPERTY(EditDefaultsOnly, Category = "Death")
	bool bDestroyAfterDeath = true;

	UPROPERTY(EditDefaultsOnly, Category = "Death", meta = (ClampMin = "0.0", ClampMax = "60.0", EditCondition = "bDestroyAfterDeath"))
	float DestroyDelay = 8.0f;

protected:
	void StartHitReact(float Duration);
	void EndHitReact();

	void StartDeath();
	void EnterRagdoll();

	// 전 클라에 재생/전환 전파
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
