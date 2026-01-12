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
	TSubclassOf<class AMageProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	int32 RequiredStage = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ProjectileSpawnDelay = 0.0f;
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

	virtual void Jump() override;
	bool bCanJump = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float JumpCooldownTimer = 0.0f;

	UFUNCTION()
	void ResetJump();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Progress")
	int32 UnlockedStageLevel = 1;
};