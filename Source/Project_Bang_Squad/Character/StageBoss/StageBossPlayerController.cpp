// Source/Project_Bang_Squad/Character/StageBoss/StageBossPlayerController.cpp
#include "StageBossPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "AQTE_Trap.h" // 생성할 트랩 헤더

AStageBossPlayerController::AStageBossPlayerController()
{
}

void AStageBossPlayerController::BeginPlay()
{
	Super::BeginPlay();
}

void AStageBossPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// 1. 기존 그룹 QTE 바인딩
		if (QTE_Action)
		{
			EIC->BindAction(QTE_Action, ETriggerEvent::Started, this, &AStageBossPlayerController::Input_QTEInteract);
		}

		// 2. 신규 개인 QTE 바인딩 (A/D 연타)
		if (IndividualQTE_Action)
		{
			// 연타이므로 Started 이벤트를 사용합니다.
			EIC->BindAction(IndividualQTE_Action, ETriggerEvent::Started, this, &AStageBossPlayerController::Input_IndividualQTEMash);
		}
	}
}

// === [기존 그룹 QTE 구현부 (생략 또는 유지)] ===
void AStageBossPlayerController::Input_QTEInteract() { Server_SubmitQTEInput(); }
void AStageBossPlayerController::Server_SubmitQTEInput_Implementation() { /* 기존 로직 */ }

// === [신규 개인 QTE 구현부] ===

void AStageBossPlayerController::Server_SetQTETrap(AQTE_Trap* InTrap)
{
	// 서버에서 트랩이 세팅되면 클라이언트의 상태를 QTE 모드로 전환합니다.
	CurrentQTETrap = InTrap;
	Client_ToggleIndividualQTEState(true);
}

void AStageBossPlayerController::Server_ClearQTETrap()
{
	// 트랩이 풀리면 컨트롤러의 상태를 원복합니다.
	CurrentQTETrap = nullptr;
	Client_ToggleIndividualQTEState(false);
}

void AStageBossPlayerController::Client_ToggleIndividualQTEState_Implementation(bool bIsActive)
{
	// 로컬 플레이어의 입력 매핑을 교체하여 다른 조작을 무시하고 QTE만 받도록 설정
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (bIsActive)
		{
			// 10이라는 높은 우선순위 부여 (다른 입력 덮어쓰기)
			if (IndividualQTE_IMC) Subsystem->AddMappingContext(IndividualQTE_IMC, 10);
		}
		else
		{
			if (IndividualQTE_IMC) Subsystem->RemoveMappingContext(IndividualQTE_IMC);
		}
	}

	// 블루프린트로 위젯 띄우기/끄기 명령 전달
	OnToggleIndividualQTEWidget(bIsActive);
}

void AStageBossPlayerController::Client_UpdateIndividualQTEUI_Implementation(int32 CurrentCount, int32 MaxCount)
{
	// 블루프린트 위젯의 진행도 바 혹은 텍스트 업데이트
	OnUpdateIndividualQTECount(CurrentCount, MaxCount);
}

void AStageBossPlayerController::Input_IndividualQTEMash(const FInputActionValue& Value)
{
	// 클라이언트가 A 또는 D를 누를 때마다 서버로 전송
	Server_SubmitIndividualQTEInput();
}

void AStageBossPlayerController::Server_SubmitIndividualQTEInput_Implementation()
{
	// 서버에서 현재 자신을 가둔 트랩에 진행도를 알림
	if (CurrentQTETrap)
	{
		CurrentQTETrap->AddQTEProgress();
	}
}