// EnemyMidBoss.h

#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBossData.h"
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"
#include "EnemyMidBoss.generated.h"

// [NEW] 전방 선언
class UBoxComponent;

// 보스 페이즈 관리용 Enum
UENUM(BlueprintType)
enum class EMidBossPhase : uint8
{
	Normal      UMETA(DisplayName = "Normal Phase"),
	Gimmick     UMETA(DisplayName = "Gimmick Phase"),
	Enraged     UMETA(DisplayName = "Enraged Phase"),
	Dead        UMETA(DisplayName = "Dead")
};

/**
 * [EnemyMidBoss] 최종 통합 버전
 * 1. Data Asset: 외형, 스탯, 애니메이션
 * 2. Network: 페이즈 동기화, 사망 처리
 * 3. AI: HealthComponent 및 AIController 연동
 * 4. Combat: 무기 충돌 박스 및 데미지 처리 (추가됨)
 */
UCLASS()
class PROJECT_BANG_SQUAD_API AEnemyMidBoss : public AEnemyCharacterBase
{
	GENERATED_BODY()

public:
	AEnemyMidBoss();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;

	// 피격 처리
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/* --- [Combat: Weapon Collision] --- */
	// AnimNotify에서 호출하여 공격 판정 켜기
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void EnableWeaponCollision();

	// AnimNotify에서 호출하여 공격 판정 끄기
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void DisableWeaponCollision();

	/* --- [Animation Helpers] --- */
	float PlayAggroAnim();
	float PlayRandomAttack();
	float PlayHitReactAnim();

protected:
	virtual void BeginPlay() override;

	// [NEW] 무기 충돌 감지 함수
	UFUNCTION()
	void OnWeaponOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	/* --- [Phase Control] --- */
public:
	void SetPhase(EMidBossPhase NewPhase);
	EMidBossPhase GetPhase() const { return CurrentPhase; }

protected:
	// --- [Components] ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHealthComponent> HealthComponent;

	// [NEW] 무기 충돌 박스
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UBoxComponent> WeaponCollisionBox;

	// --- [Data Asset] ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Data")
	TObjectPtr<UEnemyBossData> BossData;

	// --- [Combat Stats] ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackDamage = 20.0f; // 기본 공격력

	// --- [State] ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentPhase, Category = "Boss|State")
	EMidBossPhase CurrentPhase;

	UFUNCTION()
	void OnRep_CurrentPhase();

	// HealthComponent 사망 신호 처리
	UFUNCTION()
	void OnDeath();
};