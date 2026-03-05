#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
// 구조체와 Enum을 가져오기 위해 보스 데이터를 인클루드합니다.
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBossData.h" 
#include "Stage3BossAIController.generated.h"

class AStage3Boss; // 전방 선언 (컴파일 속도 최적화 및 순환 참조 방지)

UCLASS()
class PROJECT_BANG_SQUAD_API AStage3BossAIController : public AAIController
{
	GENERATED_BODY()

public:
	AStage3BossAIController();
	void ForceActionTimerDelay(float Delay);
protected:
	virtual void OnPossess(APawn* InPawn) override;

	// 타이머에 넣으려면 UFUNCTION 처리가 안전합니다.
	// [최적화/동기화] Tick 대신 TimerManager를 활용한 이벤트 드리븐 방식 적용
	UFUNCTION()
	void DecideNextAction();

	// 보스가 이동을 완료했을 때 호출되는 내장 함수 오버라이드
	virtual void OnMoveCompleted(FAIRequestID RequestID, const struct FPathFollowingResult& Result) override;

	// [안전성] GC 방지 및 언리얼 5 표준 스마트 포인터 사용
	UPROPERTY(Transient)
	TObjectPtr<AStage3Boss> Boss;

	// [최적화] 타이머 핸들 관리
	FTimerHandle ActionTimer;
	float CombatStartTime = 0.0f;

	// [아키텍처] 스킬의 '마지막 사용 시간(상태)'을 기록하는 맵
	TMap<EBoss3Skill, float> LastSkillUseTimes;

private:
	// [도움말] 쿨타임 체크를 캡슐화한 헬퍼 함수
	bool CanUseSkill(EBoss3Skill InSkill) const;
};