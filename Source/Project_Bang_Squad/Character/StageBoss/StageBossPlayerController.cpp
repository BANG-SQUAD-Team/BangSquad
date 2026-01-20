// Source/Project_Bang_Squad/Character/StageBoss/StageBossPlayerController.cpp
#include "StageBossPlayerController.h"
#include "Project_Bang_Squad/Character/StageBoss/Stage1Boss.h" // 보스 클래스 참조
#include "Kismet/GameplayStatics.h"

void AStageBossPlayerController::SendBossQTEInput()
{
	// 현재 월드에 존재하는 보스 액터를 찾습니다.
	// (보스전 맵에는 보스가 1명만 존재한다고 가정)
	AActor* BossActor = UGameplayStatics::GetActorOfClass(GetWorld(), AStage1Boss::StaticClass());

	if (AStage1Boss* Boss = Cast<AStage1Boss>(BossActor))
	{
		// 보스에게 "QTE 입력 발생!" 신호를 보냅니다. (서버로 전송됨)
		Boss->Server_SubmitQTEInput(this);

		// 디버깅용 로그 (필요 시 주석 해제)
		// UE_LOG(LogTemp, Log, TEXT("[BossPC] Sent QTE Input to Boss"));
	}

};