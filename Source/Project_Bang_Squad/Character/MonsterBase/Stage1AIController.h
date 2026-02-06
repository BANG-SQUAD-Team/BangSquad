// Source/Project_Bang_Squad/Character/MonsterBase/Stage1AIController.h

#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/MonsterBase/MidBossAIController.h"
#include "Stage1AIController.generated.h"

class AEnemyMidBoss;

// 패턴 정의 (그대로 유지)
UENUM(BlueprintType)
enum class EStage1Pattern : uint8
{
    MeleeRush,      // 평타 (근접)
    SlashStationary // 참격 (원거리)
};

UCLASS()
class PROJECT_BANG_SQUAD_API AStage1AIController : public AMidBossAIController
{
    GENERATED_BODY()

public:
    virtual void OnPossess(APawn* InPawn) override;

protected:
    // [설계 변경] 확률 계산 없이 "이번엔 뭐 할 차례인지" 결정
    virtual void StartChasing() override;

    // 참격도 거리가 멀면 추격하도록 수정
    virtual void UpdateChaseState(float DeltaTime) override;

    // 공격 중 회전 (그대로 유지)
    virtual void UpdateAttackState(float DeltaTime) override;

    // [추가] 공격이 끝나면 순서를 뒤집고(Toggle) 다시 추격 시작
    virtual void FinishAttack() override;

private:
    UPROPERTY()
    TObjectPtr<AEnemyMidBoss> MyPawn;

    // 현재 실행 중인 패턴
    EStage1Pattern CurrentPattern;

    // [핵심] 이번 턴이 참격(Slash) 차례인가? (false면 평타, true면 참격)
    bool bIsSlashTurn = false;
};