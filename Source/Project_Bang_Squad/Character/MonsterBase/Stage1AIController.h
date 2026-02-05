// Source/Project_Bang_Squad/Character/MonsterBase/Stage1AIController.h

#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/MonsterBase/MidBossAIController.h"
#include "Stage1AIController.generated.h"

class AEnemyMidBoss;

// [패턴 정의] Stage 1 보스가 수행할 행동의 종류
UENUM(BlueprintType)
enum class EStage1Pattern : uint8
{
    MeleeRush,      // 1. 달려가서 근접 공격
    SlashStationary // 2. 제자리에서 검기 발사
};

/**
 * [Stage 1 AI Controller]
 * - 특징: 'StartChasing' 단계에서 패턴을 미리 결정합니다.
 * - 참격: 제자리에서 즉시 발사.
 * - 근접: 타겟에게 달려간 뒤(Chase) 멈춰서 공격.
 */
UCLASS()
class PROJECT_BANG_SQUAD_API AStage1AIController : public AMidBossAIController
{
    GENERATED_BODY()

public:
    virtual void OnPossess(APawn* InPawn) override;

protected:
    // [핵심] 부모의 추격 명령을 가로채서 "패턴 결정(Decision)"을 수행
    virtual void StartChasing() override;

    // 근접 돌진 패턴일 때만 실행되는 로직
    virtual void UpdateChaseState(float DeltaTime) override;

    // 공격 중 회전 보정
    virtual void UpdateAttackState(float DeltaTime) override;

private:
    UPROPERTY()
    TObjectPtr<AEnemyMidBoss> MyPawn;

    // 현재 결정된 패턴 저장
    EStage1Pattern CurrentPattern;

    // --- [Skill Config] ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Combat", meta = (AllowPrivateAccess = "true"))
    float SlashSkillCooldown = 8.0f; // 검기 쿨타임

    float LastSlashTime = -100.0f; // 초기화용
};