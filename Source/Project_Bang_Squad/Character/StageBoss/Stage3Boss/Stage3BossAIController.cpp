#include "Stage3BossAIController.h"
#include "Stage3Boss.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Navigation/PathFollowingComponent.h"
void AStage3BossAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	Boss = Cast<AStage3Boss>(InPawn);

	// 왜 이렇게 짰는지: [권한 분리] AI 로직은 100% 서버에서만 돌아야 합니다. 
	// 컨트롤러는 서버에만 존재하지만, 방어적 프로그래밍을 위해 HasAuthority()로 한 번 더 감싸줍니다.
	if (HasAuthority())
	{
		// 빙의 시점(전투 시작)의 시간을 기록합니다.
		CombatStartTime = GetWorld()->GetTimeSeconds();
		GetWorldTimerManager().SetTimer(ActionTimer, this, &AStage3BossAIController::DecideNextAction, 3.0f, false);
	}
}

void AStage3BossAIController::DecideNextAction()
{
	if (!HasAuthority() || !IsValid(Boss)) return;

	UEnemyBossData* BossData = Boss->GetBossData();
	if (!IsValid(BossData))
	{
		GetWorldTimerManager().SetTimer(ActionTimer, this, &AStage3BossAIController::DecideNextAction, 1.0f, false);
		return;
	}

	// ==============================================================================
	// [1] 진짜 '플레이어'만 타겟팅 (제자리 공격 버그의 진짜 원인 해결)
	// ==============================================================================
	AActor* Target = nullptr;
	float MinDist = MAX_FLT;

	// 왜 이렇게 짰는지: GetAllActorsOfClass(ACharacter)를 쓰면 맵의 투명 더미나 미니언을 타겟으로 잡아 
	// 거리 0 판정으로 제자리 공격을 하는 버그가 생깁니다. 오직 '실제 접속한 플레이어'만 찾도록 이터레이터로 변경합니다.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->GetPawn())
		{
			APawn* PlayerPawn = PC->GetPawn();
			if (PlayerPawn->IsPendingKillPending()) continue;

			// 유저님 요청대로 덩치 보정(-BossRadius)을 빼고 DataAsset 통제를 위해 순수 거리만 사용합니다.
			float D = Boss->GetDistanceTo(PlayerPawn);
			if (D < MinDist)
			{
				MinDist = D;
				Target = PlayerPawn;
			}
		}
	}

	if (!Target)
	{
		GetWorldTimerManager().SetTimer(ActionTimer, this, &AStage3BossAIController::DecideNextAction, 1.0f, false);
		return;
	}

	// ==============================================================================
	// [2] DataAsset 사거리 가져오기 & 가중치 계산 (10초 룰 삭제)
	// ==============================================================================
	float AttackRange = BossData->AttackRange > 0.0f ? BossData->AttackRange : 300.0f;
	float Time = GetWorld()->GetTimeSeconds();
	TMap<EBoss3Skill, float> Weights;

	// 거리가 사거리(AttackRange) 안쪽일 때만 평타(Basic) 가중치 부여
	if (MinDist <= AttackRange)
	{
		// DataAsset에 Basic 설정이 있으면 그 가중치를 쓰고, 없으면 기본 100을 줍니다.
		float BasicWeight = BossData->Stage3SkillConfigs.Contains(EBoss3Skill::Basic) ? BossData->Stage3SkillConfigs[EBoss3Skill::Basic].Weight : 100.0f;
		Weights.Add(EBoss3Skill::Basic, BasicWeight);
	}

	// 나머지 스킬들은 쿨타임만 돌았으면 무조건 가중치 추가 (즉시 맹공격)
	for (const auto& Pair : BossData->Stage3SkillConfigs)
	{
		EBoss3Skill SkillType = Pair.Key;
		if (SkillType == EBoss3Skill::Basic) continue; // Basic은 거리 조건이 필요하므로 위에서 처리함

		const FBoss3SkillDetails& Config = Pair.Value;
		float LastUseTime = LastSkillUseTimes.Contains(SkillType) ? LastSkillUseTimes[SkillType] : -1000.0f;

		if (Time - LastUseTime >= Config.Cooldown)
		{
			Weights.Add(SkillType, Config.Weight);
		}
	}

	// ==============================================================================
	// [3] 룰렛(랜덤 픽)
	// ==============================================================================
	float TotalWeight = 0.0f;
	for (const auto& Pair : Weights) TotalWeight += Pair.Value;

	if (TotalWeight <= 0.0f)
	{
		Weights.Add(EBoss3Skill::Chase, 100.0f);
		TotalWeight = 100.0f;
	}

	float RandVal = FMath::FRandRange(0.0f, TotalWeight);
	EBoss3Skill Selected = EBoss3Skill::Chase;

	for (const auto& Pair : Weights)
	{
		RandVal -= Pair.Value;
		if (RandVal <= 0.0f) { Selected = Pair.Key; break; }
	}

	// ==============================================================================
	// [4] 스킬 실행
	// ==============================================================================
	float Duration = 1.0f;

	switch (Selected)
	{
	case EBoss3Skill::Basic:
		StopMovement(); // 공격할 때는 미끄러지지 않게 이동을 확실히 멈춤
		if (IsValid(Target))
		{
			FVector Dir = (Target->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal();
			Dir.Z = 0.0f;
			Boss->SetActorRotation(Dir.Rotation());
		}
		Duration = Boss->Execute_BasicAttack();
		LastSkillUseTimes.Add(EBoss3Skill::Basic, Time);
		break;

	case EBoss3Skill::Laser:
		StopMovement();
		Duration = Boss->Execute_Laser();
		LastSkillUseTimes.Add(EBoss3Skill::Laser, Time);
		break;

	case EBoss3Skill::Meteor:
		StopMovement();
		Duration = Boss->Execute_Meteor();
		LastSkillUseTimes.Add(EBoss3Skill::Meteor, Time);
		break;

	case EBoss3Skill::Break:
		StopMovement();
		Duration = Boss->Execute_PlatformBreak();
		LastSkillUseTimes.Add(EBoss3Skill::Break, Time);
		break;

	case EBoss3Skill::Chase:
		// [매우 중요한 최적화 팁] MoveToActor로 몇 초 동안 맹목적으로 쫓아가게 두지 않습니다!
		// 0.25초마다 짧게 달리면서 거리를 계속 재고, DataAsset의 사거리에 닿는 즉시 
		// 다음 틱에서 멈춰서 공격하게 만듭니다. (액션 RPG의 쫀득한 타격감의 비결)
		MoveToActor(Target, 0.0f);
		Duration = 0.25f;
		break;
	}

	// 다음 액션 예약
	GetWorldTimerManager().SetTimer(ActionTimer, this, &AStage3BossAIController::DecideNextAction, Duration + 0.1f, false);
}











void AStage3BossAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	if (Result.Code == EPathFollowingResult::Success)
	{
		// 1. 기존 타이머 제거
		GetWorldTimerManager().ClearTimer(ActionTimer);

		// 2. [수정] 무한 루프(Stack Overflow) 방지를 위해 함수를 직접 호출하지 않고, 
		// 0.1초의 아주 짧은 딜레이를 주어 콜스택을 끊어줍니다. (도착 후 0.1초 뒤 즉시 공격)
		GetWorldTimerManager().SetTimer(ActionTimer, this, &AStage3BossAIController::DecideNextAction, 0.1f, false);
	}
}