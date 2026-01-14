#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Engine/DataTable.h"
#include "InputActionValue.h"
#include "BaseCharacter.generated.h"

// 전방 선언
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UHealthComponent;
class AMageProjectile; 

/** 스킬 데이터 구조체 */
USTRUCT(BlueprintType)
struct FSkillData : public FTableRowBase
{
	GENERATED_BODY()

public:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	FName SkillName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	float Damage;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	class UAnimMontage* SkillMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	float Cooldown = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	TSubclassOf<class AActor> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	int32 RequiredStage = 0;
	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ActionDelay = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	float Duration = 0.0f;
};

UCLASS()
class PROJECT_BANG_SQUAD_API ABaseCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ABaseCharacter();
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	//  서버에서만 실행되어야 하는 데미지 처리 함수
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator, AActor* DamageCauser) override;
	
	// 공격이 가능한 상태인지 묻는 함수
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool CanAttack() const;
	
	// 공격 시작 시 쿨타임을 거는 함수
	void StartAttackCooldown();
	
	//  타이탄이 던졌는지 상태 설정 함수
	void SetThrownByTitan(bool bThrown, AActor* Thrower);

	//  잡혔을 때 상태 설정 함수
	void SetIsGrabbed(bool bGrabbed);

	//  던져진 상태 확인용 변수
	bool bWasThrownByTitan = false;

	//  나를 던진 타이탄 (데미지 주체)
	UPROPERTY()
	AActor* TitanThrower = nullptr;

protected:
	virtual void BeginPlay() override;
	
	// HealthComponent의 OnDead 신호를 받을 함수
	UFUNCTION()
	virtual void OnDeath();
	
	// 중복 사망 방지용 플래그
	bool bIsDead = false;
	
	// 죽을 때 재생할 몽타주 (에디터에서 지정)
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	UAnimMontage* DeathMontage;
	
	// 애니메이션 정지용 타이머
	FTimerHandle DeathTimerHandle;
	
	// 타이머가 끝나면 실행될 함수 
	void FreezeAnimation();

	//  착지(Landed) 이벤트 오버라이드 (낙하 데미지 및 로직 처리용)
	virtual void Landed(const FHitResult& Hit) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UHealthComponent* HealthComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	USpringArmComponent* SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* Camera;

	/* ===== 입력 에셋 ===== */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* DefaultIMC;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* MoveAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* LookAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* JumpAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* AttackAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* Skill1Action;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* Skill2Action;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* JobAbilityAction;

	/* ===== 공통 로직 함수 ===== */
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	virtual void Attack() {}
	virtual void Skill1() {}
	virtual void Skill2() {}
	virtual void JobAbility() {}

	bool IsSkillUnlocked(int32 RequiredStage);

	// 공격 쿨타임 중인지 확인
	bool bIsAttackCoolingDown = false;
	
	// 기본 공격 쿨타임 (초 단위, 자식 클래스에서 변경 가능)
	// 이 시간이 지나야 다음 공격이 나감
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackCooldownTime = 1.f;
	
	// 쿨타임 타이머 핸들
	FTimerHandle AttackCooldownTimerHandle;
	
	// 쿨타임이 끝났을 때 호출될 함수
	void ResetAttackCooldown();
	
	virtual void Jump() override;
	bool bCanJump = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float JumpCooldownTimer = 1.2f;

	UFUNCTION()
	void ResetJump();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Progress")
	int32 UnlockedStageLevel = 1;
};