#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Engine/DataTable.h"
#include "InputActionValue.h"
#include "Net/UnrealNetwork.h"
#include "Project_Bang_Squad/Shop/BangSquadShopData.h" 
#include "BaseCharacter.generated.h"

// ====================================================================================
// [전방 선언 (Forward Declarations)]
// ====================================================================================
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UHealthComponent;

// ====================================================================================
// [델리게이트 (Delegates)]
// ====================================================================================
// 스킬 쿨타임 변경 시 UI에 알리기 위한 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSkillCooldownChanged, int32, SkillIndex, float, CooldownTime);

// ====================================================================================
// [구조체 (Structs)]
// ====================================================================================
/** 스킬 정보를 담고 있는 데이터 테이블 구조체 */
USTRUCT(BlueprintType)
struct FSkillData : public FTableRowBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    FName SkillName; // 스킬의 고유 이름 (UI 표시용 등)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    float MinDamage = 0.f; // 스킬 최소 데미지

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    float MaxDamage = 0.f; // 스킬 최대 데미지
    
    // 최소~최대 데미지 사이의 랜덤값 반환 (안전장치 포함)
    float GetRandomizedDamage() const
    {
       if (MinDamage >= MaxDamage) return MinDamage;
       return FMath::RandRange(MinDamage, MaxDamage);
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    class UAnimMontage* SkillMontage = nullptr; // 스킬 사용 시 재생할 애니메이션 몽타주

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    float Cooldown = 0.0f; // 스킬 쿨타임 (초)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    TSubclassOf<class AActor> ProjectileClass = nullptr; // 투사체를 발사하는 스킬일 경우 사용할 클래스

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    int32 RequiredStage = 0; // 이 스킬이 해금되기 위해 필요한 스테이지 레벨

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    float ActionDelay = 0.0f; // 스킬 발동 전 선딜레이 (모션 싱크용)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    float Duration = 0.0f; // 버프/디버프 등 지속 시간이 필요한 경우 사용

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    UTexture2D* Icon = nullptr; // HUD 스킬창에 표시될 아이콘 이미지
};

// ====================================================================================
// [클래스 (Class)]
// ====================================================================================
UCLASS()
class PROJECT_BANG_SQUAD_API ABaseCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ABaseCharacter();

    // --------------------------------------------------------------------------------
    // [1] 생명주기 및 필수 오버라이드 (Lifecycle & Core)
    // --------------------------------------------------------------------------------
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // --------------------------------------------------------------------------------
    // [2] 전투 시스템 (Combat System)
    // --------------------------------------------------------------------------------
    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool CanAttack() const; // 현재 공격 가능한 상태인지(살아있는지, 쿨타임 아닌지) 체크
    
    void StartAttackCooldown(); // 평타 쿨타임 시작

    UFUNCTION()
    bool IsDead() { return bIsDead; }

    // --- 타이탄 잡기/투척 관련 기믹 ---
    void SetThrownByTitan(bool bThrown, AActor* Thrower);
    void SetIsGrabbed(bool bGrabbed);
    
    bool bWasThrownByTitan = false; // 타이탄에게 던져진 상태인지 여부
    UPROPERTY()
    AActor* TitanThrower = nullptr; // 나를 던진 타이탄 액터 정보

    // --------------------------------------------------------------------------------
    // [3] 이동 및 상태 이상 (Movement & Status Effects)
    // --------------------------------------------------------------------------------
    void ApplySlowDebuff(bool bActive, float Ratio); // 슬로우 장판 밟았을 때 속도 조절
    void SetWindResistance(bool bEnable); // 바람 기믹 구역 진입 시 브레이크 수치 조절
    void SetJumpRestricted(bool bRestricted); // 강제 점프 불가 상태 설정
    float GetDefaultWalkSpeed() const { return DefaultMaxWalkSpeed; }

    // 바람에 의해 공중에 떴는지 여부 (클라이언트 동기화용)
    UPROPERTY(ReplicatedUsing = OnRep_WindFloating, BlueprintReadOnly, Category = "Movement|Wind")
    bool bIsWindFloating = false;

    UFUNCTION()
    void OnRep_WindFloating();

    // --------------------------------------------------------------------------------
    // [4] 스킬 및 UI 동기화 (Skills & UI Sync)
    // --------------------------------------------------------------------------------
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnSkillCooldownChanged OnSkillCooldownChanged; // UI에 쿨타임 전달용 델리게이트

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void TriggerSkillCooldown(int32 SkillIndex, float CooldownTime);

    // 서버가 클라이언트의 UI를 갱신하도록 명령
    UFUNCTION(Client, Reliable)
    void Client_TriggerSkillCooldown(int32 SkillIndex, float CooldownTime);

    UFUNCTION(BlueprintCallable, Category = "UI")
    UTexture2D* GetSkillIconByRowName(FName RowName); // 데이터테이블에서 아이콘 추출

    UFUNCTION(BlueprintCallable, Category = "UI")
    FText GetSkillNameTextByRowName(FName RowName); // 데이터테이블에서 스킬 이름 추출
    
    bool IsSkillUnlockedByRowName(FName RowName); // 해당 스킬이 해금되었는지 체크

    // --------------------------------------------------------------------------------
    // [5] 상점 및 외형 시스템 (Shop & Appearance)
    // --------------------------------------------------------------------------------
    UFUNCTION(BlueprintCallable, Category = "Shop")
    void EquipShopItem(const FShopItemData& ItemData);

    UFUNCTION(Server, Reliable)
    void Server_EquipShopItem(const FShopItemData& ItemData);

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_EquipShopItem(const FShopItemData& ItemData);

    UFUNCTION(BlueprintCallable, Category = "Shop")
    void EquipSkin(UMaterialInterface* NewSkin);

    // PlayerState에 저장된 장착 정보를 읽어와 실제 메쉬에 적용
    void UpdateAppearanceFromPlayerState();

    // --------------------------------------------------------------------------------
    // [6] 퍼블릭 유틸리티 (Public Utilities)
    // --------------------------------------------------------------------------------
    void Zoom(const FInputActionValue& Value); // 마우스 휠을 이용한 카메라 줌인/아웃

    UFUNCTION() void ShowArenaHPBar(); // 적 머리 위 HP바 띄우기
    UFUNCTION() void HideArenaHPBar();

    // 본인 머리 위에 표시할 세모 마커 (나만 보임)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
    TObjectPtr<class UStaticMeshComponent> OverheadMarkerMesh;

protected:
    // --------------------------------------------------------------------------------
    // [1] 생명주기 및 오버라이드 (Protected Lifecycle)
    // --------------------------------------------------------------------------------
    virtual void BeginPlay() override;
    virtual void Landed(const FHitResult& Hit) override; // 땅에 착지했을 때 호출

    // --------------------------------------------------------------------------------
    // [2] 핵심 컴포넌트 (Core Components)
    // --------------------------------------------------------------------------------
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components")
    UHealthComponent* HealthComp; // 체력 관리 전용 컴포넌트

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    USpringArmComponent* SpringArm; // 카메라를 매달아두는 지지대(스프링암)

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    UCameraComponent* Camera;

    // --- 상점 아이템 부착용 메쉬 컴포넌트 ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop|Components", meta = (AllowPrivateAccess = "true"))
    class UStaticMeshComponent* HeadAccessoryComponent; // 투구, 모자 등 고정형 아이템

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop|Components", meta = (AllowPrivateAccess = "true"))
    class USkeletalMeshComponent* HeadSkeletalComp; // 망토, 꼬리 등 움직이는 아이템

    UPROPERTY(BlueprintReadWrite, Category = "Shop|Components")
    USceneComponent* ItemAttachParent; // 아이템을 부착할 부모 (메이지는 지팡이, 그 외는 캐릭터 몸)

    UPROPERTY(VisibleAnywhere)
    class UWidgetComponent* ArenaHPBarWidget; // 아레나 모드용 플로팅 HP바

    UPROPERTY()
    class UMaterialInterface* DefaultSkinMaterial;

    // --------------------------------------------------------------------------------
    // [3] 데이터 테이블 (Data Tables)
    // --------------------------------------------------------------------------------
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data")
    class UDataTable* ItemDataTable; // 상점 장비 데이터

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data")
    class UDataTable* SkinDataTable; // 상점 스킨 데이터

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data")
    class UDataTable* SkillDataTable; // 직업별 스킬 데이터

    // --------------------------------------------------------------------------------
    // [4] 입력 및 조작 (Input & Control)
    // --------------------------------------------------------------------------------
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input") UInputMappingContext* DefaultIMC;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input") UInputAction* MoveAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input") UInputAction* LookAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input") UInputAction* JumpAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input") UInputAction* AttackAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input") UInputAction* Skill1Action;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input") UInputAction* Skill2Action;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input") UInputAction* JobAbilityAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input") class UInputAction* ZoomAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input") class UInputAction* InteractionAction; // 상인 대화 등

    void Move(const FInputActionValue& Value);
    virtual void Look(const FInputActionValue& Value);
    virtual void Jump() override;
    void Interact();
    
    // --------------------------------------------------------------------------------
    // [5] 스킬 및 액션 내부 로직 (Combat Internals)
    // --------------------------------------------------------------------------------
    // 키보드 입력을 받을 가상 함수들 (자식 클래스에서 재정의)
    virtual void Attack();
    virtual void Skill1();
    virtual void Skill2();
    virtual void JobAbility();

    // 몽타주 재생을 서버/클라이언트에 동기화하는 함수 세트
    void PlayActionMontage(UAnimMontage* MontageToPlay);
    UFUNCTION(Server, Reliable) void Server_PlayActionMontage(UAnimMontage* MontageToPlay);
    UFUNCTION(NetMulticast, Reliable) void Multicast_PlayActionMontage(UAnimMontage* MontageToPlay);

    // 몽타주 강제 정지 동기화
    UFUNCTION(Server, Reliable) void Server_StopActionMontage(UAnimMontage* MontageToStop = nullptr, float BlendOutTime = 0.25f);
    UFUNCTION(NetMulticast, Reliable) void Multicast_StopActionMontage(UAnimMontage* MontageToStop = nullptr, float BlendOutTime = 0.25f);

    UFUNCTION() void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted); // 모션 종료 감지

    // --- 공격 쿨타임 및 상태 ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat") float AttackCooldownTime = 1.f;
    FTimerHandle AttackCooldownTimerHandle;
    void ResetAttackCooldown();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat") bool bIsAttacking; // 모션 재생 중인지
    bool bIsAttackCoolingDown = false; // 공격 쿨타임이 도는 중인지

    // --------------------------------------------------------------------------------
    // [6] 미니게임 및 아레나 (MiniGame & Arena)
    // --------------------------------------------------------------------------------
    void ArenaThrowAction();
    UFUNCTION(Server, Reliable) void Server_ArenaThrowAction();
    UFUNCTION() void Execute_ArenaThrowProjectile(); // 실제 투사체 생성 (타이머 지연용)
    void ResetArenaThrow();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena") UAnimMontage* ArenaThrowMontage;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena") TSubclassOf<class AActor> ArenaProjectileClass;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Arena") float ArenaThrowDelay = 0.5f;

    bool bCanArenaThrow = true;
    FTimerHandle ArenaThrowCooldownTimer;
    FTimerHandle ThrowDelayTimerHandle;

    bool bIsMiniGameMode = false; // 현재 미니게임 맵인지 여부
    UPROPERTY(EditDefaultsOnly) TSubclassOf<class UUserWidget> HPBarWidgetClass; // 아레나 HP 위젯 템플릿

    // --------------------------------------------------------------------------------
    // [7] 체력 및 사망 (Health & Death)
    // --------------------------------------------------------------------------------
    UFUNCTION() virtual void OnDeath();
    UFUNCTION(NetMulticast, Reliable) void Multicast_Death(); // 모든 화면에서 시체 처리
    void FreezeAnimation(); // 사망 몽타주가 끝난 후 포즈를 딱 굳히기

    void StartHealthRegen(); // 피격 후 일정 시간이 지나면 회복 시작
    void HealthRegenTick();  // 매 초마다 체력 조금씩 회복

    UPROPERTY(EditDefaultsOnly, Category = "Combat") UAnimMontage* DeathMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health") float RegenDelay = 2.0f; // 맞고 나서 회복 시작까지 걸리는 시간
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health") float RegenRate = 5.0f;  // 초당 회복량
    
    bool bIsDead = false;
    float RegenTickInterval = 0.5f; // 회복 계산 주기 (0.5초)
    FTimerHandle RegenDelayTimer;
    FTimerHandle RegenTickTimer;
    FTimerHandle DeathTimerHandle;

    // --------------------------------------------------------------------------------
    // [8] 기타 유틸리티 및 설정 (Misc Settings)
    // --------------------------------------------------------------------------------
    UFUNCTION(NetMulticast, Reliable) void Multicast_SetMaxWalkSpeed(float NewSpeed, float NewRatio); // 이속 동기화
    bool IsSkillUnlocked(int32 RequiredStage); // 스테이지 진행도에 따른 스킬 잠금해제 확인
    UFUNCTION() void ResetJump();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Progress") int32 UnlockedStageLevel = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement") float JumpCooldownTimer = 1.2f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop") FName AccessorySocketName;
    UPROPERTY(EditDefaultsOnly, Category = "Shop") FVector CharacterSpecificAccessoryOffset = FVector::ZeroVector; // 직업별 투구 오프셋 보정

    UPROPERTY(EditAnywhere, Category = "Camera") float MinTargetArmLength = 150.0f; // 카메라 최소 거리 (캐릭터 뚫림 방지)
    UPROPERTY(EditAnywhere, Category = "Camera") float MaxTargetArmLength = 800.0f; // 카메라 최대 거리
    UPROPERTY(EditAnywhere, Category = "Camera") float ZoomStep = 50.0f; // 휠 1칸당 줌 단위

    float DefaultMaxWalkSpeed;
    float CachedWalkSpeed = 0.0f;
    float CurrentSlowRatio = 1.0f;
    bool bJumpRestricted = false;
    bool bCanJump = true;

private:
    // --------------------------------------------------------------------------------
    // [9] 내부 숨김 로직 (Private Internals)
    // --------------------------------------------------------------------------------
    void ApplySlopeSlide(float DeltaTime); // 가파른 경사에서 미끄러지는 물리 로직

    UPROPERTY(EditAnywhere, Category = "Movement")
    float MaxSlideAccel = 230.0f; // 미끄러질 때 가속도
    
    float OriginalBrakingDeceleration = 0.0f; // 브레이크 수치 원상복구용 임시 변수
};