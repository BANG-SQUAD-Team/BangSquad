#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyCharacterBase.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API AEnemyCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	AEnemyCharacterBase();

protected:
	virtual void BeginPlay() override;

public:
	/**
	 * 피격 리액션(경직) 시작 요청
	 * - 서버에서 호출하는 것을 권장(멀티 기준)
	 * - 현재는 HealthComp 없이도 외부(무기/피격 판정)에서 호출 가능하게 열어둠
	 */
	UFUNCTION(BlueprintCallable, Category = "HitReact")
	void ReceiveHitReact();

	/** 현재 피격 경직 중인지 */
	UFUNCTION(BlueprintPure, Category = "HitReact")
	bool IsHitReacting() const { return bIsHitReacting; }

protected:
	/** 피격 몽타주 1개(몬스터마다 다르게 세팅) */
	UPROPERTY(EditDefaultsOnly, Category = "HitReact")
	TObjectPtr<UAnimMontage> HitReactMontage = nullptr;

	/** 피격 중 이동속도 배수(속도만 낮추고 정지는 안 함) */
	UPROPERTY(EditDefaultsOnly, Category = "HitReact", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float HitReactSpeedMultiplier = 0.35f;

	/** 몽타주 길이가 0이거나 짧을 때 보험 */
	UPROPERTY(EditDefaultsOnly, Category = "HitReact", meta = (ClampMin = "0.05", ClampMax = "5.0"))
	float HitReactMinDuration = 0.25f;

	/** 피격 중 재피격 무시(초기 안정 버전) */
	UPROPERTY(EditDefaultsOnly, Category = "HitReact")
	bool bIgnoreHitReactWhileActive = true;

protected:
	void StartHitReact(float Duration);
	void EndHitReact();

	/** 몽타주 재생을 전 클라에 전파(멀티 대비) */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayHitReactMontage();

private:
	bool bIsHitReacting = false;
	float DefaultMaxWalkSpeed = 0.f;
	FTimerHandle HitReactTimer;
};
