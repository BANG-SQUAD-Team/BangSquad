#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Engine/DataTable.h"
#include "InputActionValue.h"
#include "Net/UnrealNetwork.h"
#include "BaseCharacter.generated.h"

// 전방 선언
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UHealthComponent;
class AMageProjectile; 

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSkillCooldownChanged,
	int32, SkillIndex, float, CooldownTime);

/** 스킬 데이터 구조체 */
USTRUCT(BlueprintType)
struct FSkillData : public FTableRowBase
{
	GENERATED_BODY()

public:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	FName SkillName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	float Damage = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	class UAnimMontage* SkillMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	float Cooldown = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	TSubclassOf<class AActor> ProjectileClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	int32 RequiredStage = 0;
	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ActionDelay = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	float Duration = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	UTexture2D* Icon = nullptr;
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
	
	// 장판이 호출할 함수 (속도 조절 요청)
	void ApplySlowDebuff(bool bActive, float Ratio);

	UFUNCTION()
	bool IsDead() {return bIsDead;}
	
	void SetWindResistance(bool bEnable);
	
	// 원래 속도를 반환하는 함수 (WindZone에서 사용)
	float GetDefaultWalkSpeed() const { return DefaultMaxWalkSpeed; }
	
	// 네트워크 동기화
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Wind")
	bool bIsWindFloating = false;
	
	// 서버가 특정 클라이언트에게 "UI 쿨타임 돌려라" 명령하는 함수
	UFUNCTION(Client, Reliable)
	void Client_TriggerSkillCooldown(int32 SkillIndex, float CooldownTime);
	
	UFUNCTION()
	void OnRep_WindFloating();
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SetJumpRestricted(bool bRestricted);
	
	// UI에서 바인딩할 델리게이트 변수
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnSkillCooldownChanged OnSkillCooldownChanged;
	
	// 자식 클래스에서 쿨타임 시작할 때 호출할 함수
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void TriggerSkillCooldown(int32 SkillIndex, float CooldownTime);
	
	// UI가 호출할 헬퍼 함수
	UFUNCTION(BlueprintCallable, Category = "UI")
	UTexture2D* GetSkillIconByRowName(FName RowName);
	
	UFUNCTION(BlueprintCallable, Category = "UI")
	FText GetSkillNameTextByRowName(FName RowName);
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	class UStaticMeshComponent* HeadAccessoryComponent;

	virtual void BeginPlay() override;
	
	// 스킬 데이터 테이블
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data")
	class UDataTable* SkillDataTable;
	
	// 게임 시작 시 저장해둘 원래 속도
	float DefaultMaxWalkSpeed;

	// 현재 적용 중인 감속 비율 (기본값 1.0 = 감속 없음)
	float CurrentSlowRatio = 1.0f;
	
	
	// =========================================================
	//  체력 자동 재생 (Health Regen)
	// =========================================================
	// 마지막으로 데미지를 입은 후, 회복이 시작되기까지 대기 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float RegenDelay = 2.0f;
	
	// 초당 회복량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float RegenRate = 5.0f;
	
	// 1초에 몇번 회복할 것인가?
	float RegenTickInterval = 0.5f;
	
	// 내부 타이머 핸들
	FTimerHandle RegenDelayTimer;
	FTimerHandle RegenTickTimer;
	
	// 회복 시작 함수 (Delay 끝난 후 호출)
	void StartHealthRegen();
	
	// 실제 회복 함수 (Tick마다 호출)
	void HealthRegenTick();
	
	// HealthComponent의 OnDead 신호를 받을 함수
	UFUNCTION()
	virtual void OnDeath();
	
	// 중복 사망 방지용 플래그
	bool bIsDead = false;
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Death();
	
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
	virtual void Look(const FInputActionValue& Value);

	virtual void Attack() {}
	virtual void Skill1() {}
	virtual void Skill2() {}
	virtual void JobAbility() {}

	bool IsSkillUnlocked(int32 RequiredStage);
	
	// 미니게임 맵인지 확인하는 플래그
	bool bIsMiniGameMode = false;

	// 플레이어 캐릭터의 속도를 저장할 변수
	float CachedWalkSpeed = 0.0f;
	
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
	
	// 현재 공격 모션이 진행중 인지 체크하는 플래그
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	bool bIsAttacking;
	
	// 몽타주가 끝났을 때 호출될 콜백 함수
	UFUNCTION()
	void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	
	// 공격 몽타주 재생 및 상태 잠금을 전달하는 함수
	void PlayActionMontage(UAnimMontage* MontageToPlay);
	
	virtual void Jump() override;
	bool bCanJump = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float JumpCooldownTimer = 1.2f;

	UFUNCTION()
	void ResetJump();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Progress")
	int32 UnlockedStageLevel = 1;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetMaxWalkSpeed(float NewSpeed, float NewRatio);

bool bJumpRestricted = false;
	
private:
	// 원래 브레이크 수치 저장용
	float OriginalBrakingDeceleration = 0.0f;

	// 40~50도 경사에서 미끄러지는 로직 
	void ApplySlopeSlide(float DeltaTime);

	// 최대 가속도 기준값 (50도일 때의 값)
	UPROPERTY(EditAnywhere, Category = "Movement")
	float MaxSlideAccel = 230.0f;

public:
	UFUNCTION(BlueprintCallable, Category = "Shop")
	void EquipHeadAccessory(UStaticMesh* NewMesh);
};