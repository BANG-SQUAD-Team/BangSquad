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
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ToolTip="공격 시작 후 판정이 켜질 때까지의 시간 (선딜)"))
	float HitDelay = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ToolTip="판정이 켜져 있는 시간 (지속 시간)"))
	float HitDuration = 0.4f;
};


UCLASS()
class PROJECT_BANG_SQUAD_API AEnemyNormal : public AEnemyCharacterBase
{
	GENERATED_BODY()

public:
	AEnemyNormal();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
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

	// "������" ��ü�� ��� �� �´� ������ �ٽ�. �ݵ�� �߰�.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Attack")
	float AttackDamage = 10.f;

	// [변경] 기존 단순 배열 대신, 구조체 배열로 관리 (각 공격마다 타이밍 조절 가능)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Attack")
	TArray<FEnemyAttackData> AttackConfigs;
	
	// 무기 충돌 박스
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UBoxComponent* WeaponCollisionBox;

	

private:
	UPROPERTY()
	TSet<AActor*> HitVictims;
	
	TWeakObjectPtr<APawn> TargetPawn;
	FTimerHandle RepathTimer;

	bool bIsAttacking = false;
	float LastAttackTime = -9999.f;

	FTimerHandle AttackEndTimer;
	FTimerHandle CollisionEnableTimer;
	FTimerHandle CollisionDisableTimer;

	// �� ���� ����(��Ÿ�� 1ȸ)���� ������ 1���� ���� ���
	bool bDamageAppliedThisAttack = false;

	void AcquireTarget();
	void StartChase(APawn* NewTarget);
	void StopChase();
	void UpdateMoveTo();

	bool IsInAttackRange() const;

	// �������� ���� ���� (��Ƽ ����)
	UFUNCTION(Server, Reliable)
	void Server_TryAttack();
	void Server_TryAttack_Implementation();

	// ��� Ŭ�󿡼� "���õ� �ε���" ��Ÿ�� ���
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
