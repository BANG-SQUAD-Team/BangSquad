#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
// 구조체와 Enum을 가져오기 위해 보스 데이터를 인클루드합니다.
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBossData.h" 
#include "Stage3BossAIController.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API AStage3BossAIController : public AAIController
{
	GENERATED_BODY()

protected:
	virtual void OnPossess(APawn* InPawn) override;

	// 타이머에 넣으려면 UFUNCTION 처리가 안전합니다.
	UFUNCTION()
	void DecideNextAction();

	// [추가] 보스가 이동을 완료했을 때 호출되는 내장 함수 오버라이드
	virtual void OnMoveCompleted(FAIRequestID RequestID, const struct FPathFollowingResult& Result) override;

	// GC 방지를 위해 Transient 추가
	UPROPERTY(Transient)
	class AStage3Boss* Boss;

	FTimerHandle ActionTimer;
	float CombatStartTime = 0.0f;

	// [수정] 하드코딩된 쿨타임 변수(CD_Laser 등)들을 싹 지우고, 이 Map 하나로 통합 관리합니다.
	TMap<EBoss3Skill, float> LastSkillUseTimes;
};