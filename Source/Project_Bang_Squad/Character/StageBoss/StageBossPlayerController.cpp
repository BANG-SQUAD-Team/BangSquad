// Source/Project_Bang_Squad/Character/StageBoss/StageBossPlayerController.cpp

#include "Project_Bang_Squad/Character/StageBoss/StageBossPlayerController.h"
#include "Project_Bang_Squad/Character/StageBoss/StageBossGameMode.h" // [복구] 기존 게임모드 헤더
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Project_Bang_Squad/Character/StageBoss/AQTE_Trap.h"
#include "Project_Bang_Squad/UI/Stage/Boss/QTEWidget.h" // [복구] 기존 위젯 헤더

AStageBossPlayerController::AStageBossPlayerController()
{
}

void AStageBossPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// [복구 완료] 기존 QTE (G키) 입력 매핑 컨텍스트 추가 로직 복구
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (QTE_IMC)
		{
			// Priority를 1로 주어 기본 이동보다 우선순위 확보
			Subsystem->AddMappingContext(QTE_IMC, 1);
		}
	}
}

void AStageBossPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// [복구 완료] 위젯 정리 로직 복구
	if (QTEWidgetInstance && QTEWidgetInstance->IsInViewport())
	{
		QTEWidgetInstance->RemoveFromParent();
		QTEWidgetInstance = nullptr;
	}
}

void AStageBossPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// 1. [기존] 그룹 QTE 바인딩 (G키)
		if (QTE_Action)
		{
			EIC->BindAction(QTE_Action, ETriggerEvent::Started, this, &AStageBossPlayerController::Input_QTEInteract);
		}

		// 2. [신규] 개인 QTE 바인딩 (A/D 연타)
		if (IndividualQTE_Action)
		{
			EIC->BindAction(IndividualQTE_Action, ETriggerEvent::Started, this, &AStageBossPlayerController::Input_IndividualQTEMash);
		}
	}
}

// ==========================================
// [기존] Stage 1 그룹 QTE (G키)
// ==========================================
void AStageBossPlayerController::Input_QTEInteract()
{
	Server_SubmitQTEInput();
}

void AStageBossPlayerController::Server_SubmitQTEInput_Implementation()
{
	// [복구 완료] 지워졌던 GameMode 처리 로직 복구
	if (UWorld* World = GetWorld())
	{
		if (AStageBossGameMode* GM = Cast<AStageBossGameMode>(World->GetAuthGameMode()))
		{
			GM->ProcessQTEInput(this);
		}
	}
}

// ==========================================
// [신규] Stage 2 개인 QTE (A/D 연타)
// ==========================================
void AStageBossPlayerController::Server_SetQTETrap(AQTE_Trap* InTrap)
{
	CurrentQTETrap = InTrap;
	Client_ToggleIndividualQTEState(true);
}

void AStageBossPlayerController::Server_ClearQTETrap()
{
	CurrentQTETrap = nullptr;
	Client_ToggleIndividualQTEState(false);
}

void AStageBossPlayerController::Client_ToggleIndividualQTEState_Implementation(bool bIsActive)
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (bIsActive)
		{
			if (IndividualQTE_IMC) Subsystem->AddMappingContext(IndividualQTE_IMC, 10);
		}
		else
		{
			if (IndividualQTE_IMC) Subsystem->RemoveMappingContext(IndividualQTE_IMC);
		}
	}

	OnToggleIndividualQTEWidget(bIsActive);
}

void AStageBossPlayerController::Client_UpdateIndividualQTEUI_Implementation(int32 CurrentCount, int32 MaxCount)
{
	OnUpdateIndividualQTECount(CurrentCount, MaxCount);
}

void AStageBossPlayerController::Input_IndividualQTEMash(const FInputActionValue& Value)
{
	Server_SubmitIndividualQTEInput();
}

void AStageBossPlayerController::Server_SubmitIndividualQTEInput_Implementation()
{
	if (CurrentQTETrap)
	{
		CurrentQTETrap->AddQTEProgress();
	}
}