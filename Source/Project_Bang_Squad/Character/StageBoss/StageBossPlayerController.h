// Source/Project_Bang_Squad/Character/StageBoss/StageBossPlayerController.h
#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Game/Stage/StagePlayerController.h" // 부모 클래스 (일반 스테이지 기능 상속)
#include "StageBossPlayerController.generated.h"

/**
 * StageBossPlayerController
 * - 일반 스테이지 컨트롤러 기능을 그대로 사용하면서,
 * - 보스전 특화 기능(QTE 입력 전달 등)만 추가로 확장합니다.
 */
UCLASS()
class PROJECT_BANG_SQUAD_API AStageBossPlayerController : public AStagePlayerController
{
	GENERATED_BODY()

public:
	// --- [Input Action Interface] ---

	// QTE 입력(예: 스페이스바 연타)이 들어왔을 때 호출할 함수
	UFUNCTION(BlueprintCallable, Category = "Boss|Input")
	void SendBossQTEInput();

protected:
	// --- [Extension Points] ---
	// 추후 보스 체력바(BossHUD)나 연출 관련 로직이 필요하면 여기에 추가합니다.
	// virtual void BeginPlay() override;
};