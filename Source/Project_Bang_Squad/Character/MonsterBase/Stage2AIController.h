// Source/Project_Bang_Squad/Character/MonsterBase/Stage2AIController.h

#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/MonsterBase/MidBossAIController.h"
#include "Stage2AIController.generated.h"

class AStage2MidBoss;

// [NEW] 마법사 보스 행동 패턴 정의
UENUM(BlueprintType)
enum class EStage2Pattern : uint8
{
    MagicBarrage,   // 1. 원거리 마법 3회 발사
    Teleport,       // 2. 플레이어 근처로 순간이동
    MeleeChase,     // 3. (텔포 후 or 실패 시) 근접 추격
    MeleeAttack     // 4. 근접 공격 후 타겟 변경
};

UCLASS()
class PROJECT_BANG_SQUAD_API AStage2AIController : public AMidBossAIController
{
    GENERATED_BODY()

public:
    virtual void OnPossess(APawn* InPawn) override;
    virtual void Tick(float DeltaTime) override;

protected:
    // 부모의 추격 시작을 가로채서 패턴 초기화
    virtual void StartChasing() override;

    // 메인 패턴 로직
    void UpdateMagePattern(float DeltaTime);

private:
    UPROPERTY()
    TObjectPtr<AStage2MidBoss> MyPawn;

    // 현재 진행 중인 패턴
    EStage2Pattern CurrentPattern;

    // 마법 발사 횟수 카운터
    int32 MagicFireCount = 0;

    // 쿨타임 타이머
    float PatternCooldown = 0.0f;
};