#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h"
#include "EnemyNormal.generated.h"

class UAnimMontage;
class UBoxComponent;

USTRUCT(BlueprintType)
struct FEnemyAttackData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ToolTip = "공격 시작 후 판정이 켜질 때까지의 시간 (선딜)"))
	float HitDelay = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ToolTip = "판정이 켜져 있는 시간 (지속 시간)"))
	float HitDuration = 0.4f;
};

UCLASS()
class PROJECT_BANG_SQUAD_API AEnemyNormal : public AEnemyCharacterBase
{
	GENERATED_BODY()

public:
	AEnemyNormal();

	// [추가] Tick 함수 오버라이드 (디버그 박스 그리기용)
	virtual void Tick(float DeltaTime) override;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Attack")
	float AttackDamage = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Attack")
	TArray<FEnemyAttackData> AttackConfigs;

	// 무기 충돌 박스
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UBoxComponent* WeaponCollisionBox;

	// [추가] 가장 가까운 살아있는 플레이어를 찾는 함수 선언
	APawn* FindNearestLivingPlayer();

private:
	TWeakObjectPtr<APawn> TargetPawn;
	FTimerHandle RepathTimer;

	bool bIsAttacking = false;
	float LastAttackTime = -9999.f;

	FTimerHandle AttackEndTimer;
	FTimerHandle CollisionEnableTimer;
	FTimerHandle CollisionDisableTimer;

	// [추가] 중복 피격 방지를 위한 배열
	UPROPERTY()
	TArray<AActor*> HitVictims;

	void AcquireTarget();
	void StartChase(APawn* NewTarget);
	void StopChase();
	void UpdateMoveTo();

	bool IsInAttackRange() const;

	UFUNCTION(Server, Reliable)
	void Server_TryAttack();
	void Server_TryAttack_Implementation();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayAttackMontage(int32 MontageIndex);
	void Multicast_PlayAttackMontage_Implementation(int32 MontageIndex);

	void EndAttack();

	void EnableWeaponCollision();
	void DisableWeaponCollision();

	UFUNCTION()
	void OnWeaponOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);
};