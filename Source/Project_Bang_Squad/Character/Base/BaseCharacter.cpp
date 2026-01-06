
#include "BaseCharacter.h" 

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

// Enhanced Input 사용을 위한 필수 헤더들
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

ABaseCharacter::ABaseCharacter()
{
	// 매 프레임 Tick 함수를 호출할지 여부 (필요 없으면 false로 성능 최적화 가능)
	PrimaryActorTick.bCanEverTick = true;

	/* ===== 캐릭터 회전 및 이동 기본 설정 ===== */

	// 컨트롤러(마우스) 회전값이 캐릭터 자체의 회전에 직접 영향을 주지 않도록 설정 (3인칭 기본)
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	// 이동하는 방향으로 캐릭터 몸체가 부드럽게 회전하도록 설정
	MoveComp->bOrientRotationToMovement = true;
	// 회전 속도 설정
	MoveComp->RotationRate = FRotator(0.f, 720.f, 0.f);
	// 기본 걷기 속도
	MoveComp->MaxWalkSpeed = 600.f;

	/* ===== SpringArm (카메라 지지대) 설정 ===== */

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetRootComponent());
	SpringArm->TargetArmLength = 450.f;          // 캐릭터와의 거리
	SpringArm->bUsePawnControlRotation = true;    // 마우스 입력에 따라 지지대가 회전함
	SpringArm->bEnableCameraLag = true;           // 카메라가 캐릭터를 부드럽게 따라오도록 지연 효과
	SpringArm->CameraLagSpeed = 10.f;             // 지연 속도

	/* ===== Camera 설정 ===== */

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	// 카메라를 지지대 끝에 부착
	Camera->SetupAttachment(SpringArm);
	// 지지대가 회전하므로 카메라 자체는 컨트롤러 회전을 직접 따라갈 필요 없음
	Camera->bUsePawnControlRotation = false;
}

void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 입력 매핑 컨텍스트(IMC)를 시스템에 등록 (로컬 플레이어일 때만 작동)
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
			{
				if (DefaultIMC)
				{
					// 우선순위 0으로 IMC 추가
					Subsystem->AddMappingContext(DefaultIMC, 0);
				}
			}
		}
	}
}

void ABaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// 입력을 함수에 바인딩
void ABaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 기본 InputComponent를 Enhanced 버전으로 캐스팅
	UEnhancedInputComponent* EIC = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);

	// 각 액션 에셋이 유효하면 함수와 연결
	if (MoveAction) // Move
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABaseCharacter::Move);
	}

	if (LookAction) // Look
	{
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABaseCharacter::Look);
	}

	// 점프 액션 바인딩
	if (JumpAction)
	{
		// 키를 눌렀을 때 (Started) 엔진 기본 Jump 함수 실행
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ABaseCharacter::Jump);

		// 키에서 손을 뗐을 때 (Completed) 엔진 기본 StopJumping 함수 실행
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ABaseCharacter::StopJumping);
	}

	if (Skill1Action)
	{
		EIC->BindAction(Skill1Action, ETriggerEvent::Started, this, &ABaseCharacter::Skill1);
	}

	if (Skill2Action)
	{
		EIC->BindAction(Skill2Action, ETriggerEvent::Started, this, &ABaseCharacter::Skill2);
	}

	if (JobAbilityAction)
	{
		EIC->BindAction(JobAbilityAction, ETriggerEvent::Started, this, &ABaseCharacter::JobAbility);
	}
}

void ABaseCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();

	if (!Controller) return;

	// 카메라(컨트롤러)가 바라보는 방향
	const FRotator ControlRot = Controller->GetControlRotation();
	const FRotator YawRot(0.f, ControlRot.Yaw, 0.f);

	// 카메라 기준 방향 벡터
	const FVector ForwardDir = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector RightDir = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDir, Input.Y);
	AddMovementInput(RightDir, Input.X);
}

void ABaseCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();

	// 좌우(Yaw)는 그대로
	AddControllerYawInput(Input.X);

	// 상하(Pitch)는 반전
	AddControllerPitchInput(-Input.Y);
}