#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Engine/DataTable.h" // 데이터 테이블 기능을 사용하기 위해 추가
#include "InputActionValue.h"
#include "BaseCharacter.generated.h"

// 전방 선언
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UHealthComponent;

/** * 스킬 데이터를 정의하는 구조체
 * 데이터 테이블의 양식이 됩니다.
 */
USTRUCT(BlueprintType)
struct FSkillData : public FTableRowBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    FName SkillName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    float Damage;

    /** 스킬 시전 시 재생할 애니메이션 몽타주 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    class UAnimMontage* SkillMontage;

    // /** 스킬이 발사핧 투사체 블루프린트 클래스 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    TSubclassOf<class AMageProjectile> ProjectileClass;
    
    /** 해금 조건: 0은 즉시(JobAbility), 1 이상은 해당 스테이지 진입/클리어 시 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    int32 RequiredStage = 0;
};

UCLASS()
class PROJECT_BANG_SQUAD_API ABaseCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ABaseCharacter();
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
    virtual void BeginPlay() override;

    // 모든 플레이어 캐릭터가 공통으로 가질 체력 컴포넌트
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components")
    UHealthComponent* HealthComp;
    
    /* ===== 카메라 컴포넌트 ===== */
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

    /** 스킬 해금 여부를 확인하는 공통 함수 (추가) */
    bool IsSkillUnlocked(int32 RequiredStage);

    /* ===== 점프 시스템 (기존 유지) ===== */
    virtual void Jump() override;
    bool bCanJump = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float JumpCooldownTimer = 0.0f;

    UFUNCTION() // 타이머용 함수는 UFUNCTION이 안전합니다.
    void ResetJump();

    /* ===== 게임 진행 상태 (추가) ===== */

    /** 현재 캐릭터가 해금한 스테이지 레벨 (기본값 1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Progress")
    int32 UnlockedStageLevel = 1;
};