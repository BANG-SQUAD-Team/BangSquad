#include "Stage3BossAIController.h"
#include "Stage3Boss.h"
#include "Kismet/GameplayStatics.h"

void AStage3BossAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	Boss = Cast<AStage3Boss>(InPawn);

	// 빙의 시점(전투 시작)의 시간을 기록합니다.
	CombatStartTime = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(ActionTimer, this, &AStage3BossAIController::DecideNextAction, 3.0f, false);
}

void AStage3BossAIController::DecideNextAction()
{
	if (!Boss || !HasAuthority()) return;

	// 0번 플레이어가 아닌, 맵에 있는 모든 플레이어 중 가장 가까운 플레이어를 찾습니다.
	AActor* ClosestPlayer = nullptr;
	float MinDist = MAX_FLT;
	TArray<AActor*> Players;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), Players);

	for (AActor* P : Players)
	{
		float D = Boss->GetDistanceTo(P);
		if (D < MinDist)
		{
			MinDist = D;
			ClosestPlayer = P;
		}
	}

	if (!ClosestPlayer) return; // 타겟이 없으면 종료

	float Time = GetWorld()->GetTimeSeconds();
	float ElapsedTime = Time - CombatStartTime;

	TMap<EBoss3Skill, float> Weights;

	// [수정] 기획서 반영: 10초 이전에는 기본공격/추격만 사용
	if (ElapsedTime < 10.0f)
	{
		if (MinDist <= 300.0f) Weights.Add(EBoss3Skill::Basic, 100.0f);
	}
	else
	{
		// 10초 이후 정상 패턴 가동
		if (MinDist <= 300.0f) Weights.Add(EBoss3Skill::Basic, 100.0f);
		else Weights.Add(EBoss3Skill::Basic, 0.0f);

		if (Time - LastTime_Laser >= CD_Laser) Weights.Add(EBoss3Skill::Laser, 60.0f);
		if (Time - LastTime_Meteor >= CD_Meteor) Weights.Add(EBoss3Skill::Meteor, 80.0f);
		if (Time - LastTime_Break >= CD_Break) Weights.Add(EBoss3Skill::Break, 50.0f);
	}

	// 가중치 계산 (기존 로직 동일)
	float TotalWeight = 0.0f;
	for (auto& Pair : Weights) TotalWeight += Pair.Value;

	if (TotalWeight <= 0.0f)
	{
		Weights.Add(EBoss3Skill::Chase, 100.0f);
		TotalWeight = 100.0f;
	}

	float RandVal = FMath::FRandRange(0.0f, TotalWeight);
	EBoss3Skill Selected = EBoss3Skill::Chase;

	for (auto& Pair : Weights)
	{
		RandVal -= Pair.Value;
		if (RandVal <= 0.0f) { Selected = Pair.Key; break; }
	}

	float Duration = 1.0f;
	switch (Selected)
	{
	case EBoss3Skill::Basic: Duration = Boss->Execute_BasicAttack(); break;
	case EBoss3Skill::Laser: Duration = Boss->Execute_Laser(); LastTime_Laser = Time; break;
	case EBoss3Skill::Meteor: Duration = Boss->Execute_Meteor(); LastTime_Meteor = Time; break;
	case EBoss3Skill::Break: Duration = Boss->Execute_PlatformBreak(); LastTime_Break = Time; break;
	case EBoss3Skill::Chase: MoveToActor(ClosestPlayer, 200.0f); Duration = 1.0f; break; // ClosestPlayer 추적
	}

	GetWorldTimerManager().SetTimer(ActionTimer, this, &AStage3BossAIController::DecideNextAction, Duration + 0.5f, false);
}