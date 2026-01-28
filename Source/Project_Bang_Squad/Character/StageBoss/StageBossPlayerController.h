// Source/Project_Bang_Squad/Character/StageBoss/StageBossPlayerController.h

#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Game/Stage/StagePlayerController.h" // 부모 클래스 확인 필요
#include "StageBossPlayerController.generated.h"

/**
 * 보스전 전용 컨트롤러
 * 역할: QTE 입력(G키)을 감지해서 GameMode로 전달
 */
UCLASS()
class PROJECT_BANG_SQUAD_API AStageBossPlayerController : public AStagePlayerController
{
	GENERATED_BODY()

protected:
	virtual void SetupInputComponent() override;

	// [로컬] 키 입력 시 호출
	void Input_QTEInteract();

	// [서버] 서버로 입력 신호 전송 (RPC)
	UFUNCTION(Server, Reliable)
	void Server_SubmitQTEInput();
};