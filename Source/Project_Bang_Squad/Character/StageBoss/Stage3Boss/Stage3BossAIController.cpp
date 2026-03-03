#include "Stage3BossAIController.h"
#include "Stage3Boss.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
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

	AActor* Target = nullptr;
	float MinDist = MAX_FLT;

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->GetPawn())
		{
			APawn* PlayerPawn = PC->GetPawn();
			if (PlayerPawn->IsPendingKillPending()) continue;

		
			ABaseCharacter* BaseChar = Cast<ABaseCharacter>(PlayerPawn);
			if (BaseChar && BaseChar->IsDead()) continue;

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

	float AttackRange = BossData->AttackRange > 0.0f ? BossData->AttackRange : 300.0f;
	float Time = GetWorld()->GetTimeSeconds();
	TMap<EBoss3Skill, float> Weights;

	// ==============================================================================
	// 🟢 [수정됨] 평타(Basic)도 데이터 에셋의 쿨타임을 철저하게 따르도록 변경!
	// ==============================================================================
	if (MinDist <= AttackRange)
	{
		float BasicCooldown = BossData->Stage3SkillConfigs.Contains(EBoss3Skill::Basic) ? BossData->Stage3SkillConfigs[EBoss3Skill::Basic].Cooldown : 2.0f; // 기본 2초
		float LastBasicTime = LastSkillUseTimes.Contains(EBoss3Skill::Basic) ? LastSkillUseTimes[EBoss3Skill::Basic] : -1000.0f;

		// 평타도 쿨타임이 지나야만 가중치에 추가됨
		if (Time - LastBasicTime >= BasicCooldown)
		{
			float BasicWeight = BossData->Stage3SkillConfigs.Contains(EBoss3Skill::Basic) ? BossData->Stage3SkillConfigs[EBoss3Skill::Basic].Weight : 100.0f;
			Weights.Add(EBoss3Skill::Basic, BasicWeight);
		}
	}

	// ==============================================================================
	// 🟢 [수정됨] 특수 스킬 쿨타임 체크 (이제 Basic 제외 처리 없이 깔끔하게 체크)
	// ==============================================================================
	for (const auto& Pair : BossData->Stage3SkillConfigs)
	{
		EBoss3Skill SkillType = Pair.Key;
		if (SkillType == EBoss3Skill::Basic) continue; // Basic은 거리 조건이 있으므로 위에서 처리 완료

		const FBoss3SkillDetails& Config = Pair.Value;
		float LastUseTime = LastSkillUseTimes.Contains(SkillType) ? LastSkillUseTimes[SkillType] : -1000.0f;

		if (Time - LastUseTime >= Config.Cooldown)
		{
			Weights.Add(SkillType, Config.Weight);
		}
	}

	// 룰렛(가중치 랜덤 픽) 실행
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
	// 🟢 [수정됨] 스킬 실행 후 딜레이 추가 (스킬 연사 방지)
	// ==============================================================================
	float Duration = 1.0f;
	float PostSkillDelay = 1.5f; // 스킬을 쓴 후 최소 1.5초는 멍때리게 만듦 (숨고르기)

	switch (Selected)
	{
	case EBoss3Skill::Basic:
		StopMovement();
		if (IsValid(Target))
		{
			FVector Dir = (Target->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal();
			Dir.Z = 0.0f;
			Boss->SetActorRotation(Dir.Rotation());
		}
		Duration = Boss->Execute_BasicAttack() + PostSkillDelay; // 액션 시간에 딜레이 추가
		LastSkillUseTimes.Add(EBoss3Skill::Basic, Time);
		break;

	case EBoss3Skill::Laser:
		StopMovement();
		Duration = Boss->Execute_Laser() + PostSkillDelay;
		LastSkillUseTimes.Add(EBoss3Skill::Laser, Time);
		break;

	case EBoss3Skill::Meteor:
		StopMovement();
		Duration = Boss->Execute_Meteor() + PostSkillDelay;
		LastSkillUseTimes.Add(EBoss3Skill::Meteor, Time);
		break;

	case EBoss3Skill::Break:
		StopMovement();
		Duration = Boss->Execute_PlatformBreak() + PostSkillDelay;
		LastSkillUseTimes.Add(EBoss3Skill::Break, Time);
		break;

	case EBoss3Skill::Chase:
		MoveToActor(Target, 0.0f);
		Duration = 0.25f; // Chase는 계속 판단해야 하므로 딜레이 없음
		break;
	}

	// 다음 액션 예약
	GetWorldTimerManager().SetTimer(ActionTimer, this, &AStage3BossAIController::DecideNextAction, Duration, false);
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