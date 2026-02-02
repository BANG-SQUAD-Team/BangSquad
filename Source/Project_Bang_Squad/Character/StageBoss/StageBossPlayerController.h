// Source/Project_Bang_Squad/Character/StageBoss/StageBossPlayerController.h

#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Game/Stage/StagePlayerController.h"
#include "StageBossPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;

/**
 * 보스전 전용 컨트롤러
 * 역할: BaseCharacter 수정 없이, 독자적으로 QTE 입력을 받아 처리함.
 */
UCLASS()
class PROJECT_BANG_SQUAD_API AStageBossPlayerController : public AStagePlayerController
{
	GENERATED_BODY()

public:
	AStageBossPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	// [입력 설정] 에디터에서 할당 필요
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> QTE_IMC; // QTE 전용 키 매핑 (G키)

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> QTE_Action;      // QTE 입력 액션

	// [로컬] 키 입력 시 호출
	void Input_QTEInteract();

	// [서버] 서버로 입력 신호 전송 (RPC)
	UFUNCTION(Server, Reliable)
	void Server_SubmitQTEInput();
};