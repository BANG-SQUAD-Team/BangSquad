// Source/Project_Bang_Squad/Character/StageBoss/StageBossPlayerController.cpp

#include "StageBossPlayerController.h"
#include "StageBossGameMode.h" 
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"

AStageBossPlayerController::AStageBossPlayerController()
{
	// 생성자 로직 (필요시 추가)
}

void AStageBossPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// [핵심] QTE용 입력 매핑 컨텍스트 추가 (우선순위를 높게 설정)
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (QTE_IMC)
		{
			// Priority를 1로 설정하여 기본 캐릭터 입력(보통 0)보다 우선순위를 높임
			// 이렇게 하면 캐릭터의 다른 키와 겹쳐도 QTE가 먼저 반응함
			Subsystem->AddMappingContext(QTE_IMC, 1);
		}
	}
}

void AStageBossPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// [수정] Enhanced Input 시스템을 사용하여 바인딩
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (QTE_Action)
		{
			EIC->BindAction(QTE_Action, ETriggerEvent::Started, this, &AStageBossPlayerController::Input_QTEInteract);
		}
	}
}

void AStageBossPlayerController::Input_QTEInteract()
{
	// 로컬 클라이언트에서 G키 누름 -> 서버로 보고
	// (여기서 클라이언트 측 UI 반응이나 사운드를 즉시 재생하면 반응성이 더 좋아짐)
	Server_SubmitQTEInput();
}

void AStageBossPlayerController::Server_SubmitQTEInput_Implementation()
{
	// [서버 로직] GameMode에게 보고
	if (UWorld* World = GetWorld())
	{
		if (AStageBossGameMode* GM = Cast<AStageBossGameMode>(World->GetAuthGameMode()))
		{
			// 컨트롤러(this) 정보를 넘겨주어 누가 눌렀는지 식별 가능하게 함
			GM->ProcessQTEInput(this);

			// 디버깅 로그 (성공 시 화면에 출력됨)
			// UE_LOG(LogTemp, Log, TEXT("Player Controller Sent QTE Input!")); 
		}
	}
}