#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
// Enhanced Input의 데이터를 처리하기 위한 구조체가 포함된 헤더
#include "InputActionValue.h" 
#include "BaseCharacter.generated.h"

// 전방 선언: 컴파일 속도를 높이기 위해 클래스 이름만 미리 알려줌
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;

UCLASS()
class PROJECT_BANG_SQUAD_API ABaseCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// 생성자: 객체가 생성될 때 초기 설정을 담당
	ABaseCharacter();

protected:
	// 게임이 시작되거나 캐릭터가 스폰될 때 한 번 호출
	virtual void BeginPlay() override;

public:
	// 매 프레임마다 호출 (현재는 로직이 없으면 꺼두는 것이 성능에 좋음)
	virtual void Tick(float DeltaTime) override;

	// 입력과 함수를 연결(바인딩)하기 위한 설정
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;


protected:
	/* ===== 카메라 컴포넌트 ===== */

	// 캐릭터와 카메라 사이의 거리를 유지하고 장애물을 처리하는 '팔' 역할
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	USpringArmComponent* SpringArm;

	// 플레이어의 화면을 담당하는 실제 카메라
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* Camera;

	/* ===== 입력 에셋 (에디터 할당용) ===== */

	// 어떤 키를 누를지 정의한 매핑 컨텍스트
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* DefaultIMC;

	// 이동 입력 액션 에셋
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* MoveAction;

	// 시야 회전 입력 액션 에셋
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* LookAction;

	// 점프 액션 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* JumpAction;

	/* ===== 스킬 공통 입력 액션 ===== */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* Skill1Action; //1번 키

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* Skill2Action; //2번 키

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* JobAbilityAction; // 우클릭 (직업 능력)

protected:
	// 실제 이동 로직을 처리할 함수
	void Move(const FInputActionValue& Value);

	// 실제 카메라 회전 로직을 처리할 함수
	void Look(const FInputActionValue& Value);

	// 자식 클래스에서 실제 내용 채울 가상 함수
	virtual void Skill1() {}
	virtual void Skill2() {}
	virtual void JobAbility() {} // 직업 능력

protected:
	/** 엔진 기본 Jump를 오버라이드하여 제어 로직 추가 */
	virtual void Jump() override;

	/** 점프 가능 상태 플래그 */
	bool bCanJump = true;

	/** 점프 후 다음 점프까지의 대기 시간 (메이지는 1.33초 설정 예정) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float JumpCooldownTimer = 0.0f;

	/** 점프 제한을 해제하는 함수 */
	void ResetJump();
};