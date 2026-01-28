// Source/Project_Bang_Squad/Character/StageBoss/StageBossPlayerController.cpp

#include "StageBossPlayerController.h"
#include "StageBossGameMode.h" // [핵심] 게임모드 헤더 포함

void AStageBossPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// "Interact" 혹은 "G" 키 바인딩 (프로젝트 세팅에 맞게 이름 수정하세요)
	// 예: InputComponent->BindAction("Interact", IE_Pressed, this, &AStageBossPlayerController::Input_QTEInteract);
	// 만약 Enhanced Input을 쓰신다면 그에 맞게 작성해야 합니다.

	// (임시) 예제용 레거시 바인딩
	InputComponent->BindKey(EKeys::G, IE_Pressed, this, &AStageBossPlayerController::Input_QTEInteract);
}

void AStageBossPlayerController::Input_QTEInteract()
{
	// 로컬 클라이언트에서 입력 감지 -> 서버로 전송
	Server_SubmitQTEInput();
}

void AStageBossPlayerController::Server_SubmitQTEInput_Implementation()
{
	// [서버 로직]
	// 보스를 찾지 말고, 권한이 있는 GameMode(심판)를 찾아서 보고합니다.
	if (AStageBossGameMode* GM = GetWorld()->GetAuthGameMode<AStageBossGameMode>())
	{
		// "심판님, 저(this) 버튼 눌렀습니다!"
		GM->ProcessQTEInput(this);
	}
}