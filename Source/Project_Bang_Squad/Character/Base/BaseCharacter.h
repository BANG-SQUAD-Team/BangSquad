#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Engine/DataTable.h"
#include "InputActionValue.h"
#include "Net/UnrealNetwork.h"
// [중요] 경로가 안 맞으면 "Project_Bang_Squad/Shop/" 부분을 지우고 "BangSquadShopData.h"로 바꾸세요.
#include "Project_Bang_Squad/Shop/BangSquadShopData.h" 
#include "BaseCharacter.generated.h"

// 전방 선언
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UHealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSkillCooldownChanged, int32, SkillIndex, float, CooldownTime);

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
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// =========================================================
	//  [상점 시스템 핵심 함수]
	// =========================================================
	UFUNCTION(BlueprintCallable, Category = "Shop")
	void EquipShopItem(const FShopItemData& ItemData);

	UFUNCTION(Server, Reliable)
	void Server_EquipShopItem(const FShopItemData& ItemData);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_EquipShopItem(const FShopItemData& ItemData);

	// 전투 관련
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool CanAttack() const;
	void StartAttackCooldown();

	// 타이탄 투척 관련
	void SetThrownByTitan(bool bThrown, AActor* Thrower);
	void SetIsGrabbed(bool bGrabbed);
	bool bWasThrownByTitan = false;
	UPROPERTY()
	AActor* TitanThrower = nullptr;

	// 장판 디버프
	void ApplySlowDebuff(bool bActive, float Ratio);

	UFUNCTION()
	bool IsDead() { return bIsDead; }

	void SetWindResistance(bool bEnable);
	float GetDefaultWalkSpeed() const { return DefaultMaxWalkSpeed; }

	// 네트워크 동기화
	UPROPERTY(ReplicatedUsing = OnRep_WindFloating, BlueprintReadOnly, Category = "Wind")
	bool bIsWindFloating = false;

	UFUNCTION()
	void OnRep_WindFloating();

	UFUNCTION(Client, Reliable)
	void Client_TriggerSkillCooldown(int32 SkillIndex, float CooldownTime);

	void SetJumpRestricted(bool bRestricted);

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnSkillCooldownChanged OnSkillCooldownChanged;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void TriggerSkillCooldown(int32 SkillIndex, float CooldownTime);

	UFUNCTION(BlueprintCallable, Category = "UI")
	UTexture2D* GetSkillIconByRowName(FName RowName);

	UFUNCTION(BlueprintCallable, Category = "UI")
	FText GetSkillNameTextByRowName(FName RowName);
	bool IsSkillUnlockedByRowName(FName RowName);
	
	// 줌 입력 액션
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	class UInputAction* ZoomAction;

	// 2. 줌 처리 함수
	void Zoom(const FInputActionValue& Value);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	TObjectPtr<class UStaticMeshComponent> OverheadMarkerMesh;

	//상호작용 F키
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	class UInputAction* InteractionAction;

protected:
	virtual void BeginPlay() override;
	virtual void Landed(const FHitResult& Hit) override;

	// =========================================================
	//  [상점 시스템 컴포넌트]
	// =========================================================
	// 1. 스태틱 메쉬용 (투구 등)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	class UStaticMeshComponent* HeadAccessoryComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Socket")
	FName AccessorySocketName;

	// 2. 스켈레탈 메쉬용 (망토, 꼬리 등)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	class USkeletalMeshComponent* HeadSkeletalComp;

	// 데이터 테이블
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data")
	class UDataTable* SkillDataTable;

	// 컴포넌트들
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

	/* ===== 내부 로직 ===== */
	void Move(const FInputActionValue& Value);
	virtual void Look(const FInputActionValue& Value);
	virtual void Jump() override;
	virtual void Attack() {}
	virtual void Skill1() {}
	virtual void Skill2() {}
	virtual void JobAbility() {}

	bool IsSkillUnlocked(int32 RequiredStage);

	bool bIsMiniGameMode = false;
	float DefaultMaxWalkSpeed;
	float CachedWalkSpeed = 0.0f;
	float CurrentSlowRatio = 1.0f;
	bool bIsAttackCoolingDown = false;
	bool bJumpRestricted = false;
	bool bCanJump = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackCooldownTime = 1.f;
	FTimerHandle AttackCooldownTimerHandle;
	void ResetAttackCooldown();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	bool bIsAttacking;
	UFUNCTION()
	void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void PlayActionMontage(UAnimMontage* MontageToPlay);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float JumpCooldownTimer = 1.2f;
	UFUNCTION()
	void ResetJump();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Progress")
	int32 UnlockedStageLevel = 1;

	// 체력 회복
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float RegenDelay = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float RegenRate = 5.0f;
	float RegenTickInterval = 0.5f;
	FTimerHandle RegenDelayTimer;
	FTimerHandle RegenTickTimer;
	void StartHealthRegen();
	void HealthRegenTick();

	// 사망 처리
	UFUNCTION()
	virtual void OnDeath();
	bool bIsDead = false;
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Death();
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	UAnimMontage* DeathMontage;
	FTimerHandle DeathTimerHandle;
	void FreezeAnimation();

	// 동기화 함수
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetMaxWalkSpeed(float NewSpeed, float NewRatio);

	UPROPERTY(EditAnywhere, Category = "Camera")
	float MinTargetArmLength = 150.0f; // 너무 가까우면 캐릭터 뚫음

	UPROPERTY(EditAnywhere, Category = "Camera")
	float MaxTargetArmLength = 800.0f; // 너무 멀면 안 보임

	UPROPERTY(EditAnywhere, Category = "Camera")
	float ZoomStep = 50.0f; // 휠 한 칸당 이동 거리

	void Interact();//상호작용 함수
	
private:
	float OriginalBrakingDeceleration = 0.0f;
	void ApplySlopeSlide(float DeltaTime);
	UPROPERTY(EditAnywhere, Category = "Movement")
	float MaxSlideAccel = 230.0f;
};