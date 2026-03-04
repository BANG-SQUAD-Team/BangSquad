#include "Stage3BossAIController.h"
#include "Project_Bang_Squad/Character/StageBoss/Stage3Boss/Stage3Boss.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Navigation/PathFollowingComponent.h"

AStage3BossAIController::AStage3BossAIController()
{
	// 왜 이렇게 짰는지: [멀티플레이어 동기화] 
	// AIController는 기본적으로 서버에만 존재하지만, 향후 멀티플레이 환경에서 
	// AI가 플레이어의 상태(PlayerState)나 어그로 수치를 참조해야 할 때를 대비해 활성화해 둡니다.
	bWantsPlayerState = true;
}

void AStage3BossAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// 왜 이렇게 짰는지: [안전성] C++ 캐스팅 후 유효성 검사는 필수입니다.
	Boss = Cast<AStage3Boss>(InPawn);
	if (!IsValid(Boss)) return;

	// 왜 이렇게 짰는지: [권한 분리] AI 로직은 100% 서버에서만 돌아야 합니다. 
	// 컨트롤러는 서버에만 존재하지만, 방어적 프로그래밍을 위해 HasAuthority()로 한 번 더 감싸줍니다.
	if (HasAuthority())
	{
		CombatStartTime = GetWorld()->GetTimeSeconds();
		// 왜 이렇게 짰는지: [최적화] Tick 대신 타이머를 사용하여 서버 CPU 부하를 줄입니다.
		GetWorldTimerManager().SetTimer(ActionTimer, this, &AStage3BossAIController::DecideNextAction, 3.0f, false);
	}
}

void AStage3BossAIController::DecideNextAction()
{
	// 왜 이렇게 짰는지: [안전성] 타이머 델리게이트 실행 시점에 보스가 죽었거나 파괴되었을 수 있으므로 검증합니다.
	if (!HasAuthority() || !IsValid(Boss)) return;

	UEnemyBossData* BossData = Boss->GetBossData();
	if (!IsValid(BossData))
	{
		GetWorldTimerManager().SetTimer(ActionTimer, this, &AStage3BossAIController::DecideNextAction, 1.0f, false);
		return;
	}

	AActor* Target = nullptr;

	// 🟢 [핵심 수정 1] 기존 MinDist 삭제 -> '표면 간 거리'를 추적할 EdgeToEdgeDist로 교체
	float EdgeToEdgeDist = MAX_FLT;

	// 왜 이렇게 짰는지: [최적화] 4인 멀티플레이어 환경이므로 전체 순회(O(N)) 비용이 매우 적습니다.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (IsValid(PC) && IsValid(PC->GetPawn())) // IsPendingKillPending() 대신 IsValid() 사용 (UE5 표준)
		{
			APawn* PlayerPawn = PC->GetPawn();

			ABaseCharacter* BaseChar = Cast<ABaseCharacter>(PlayerPawn);
			// 왜 이렇게 짰는지: [AI 판단] 죽은 플레이어는 타겟팅에서 제외하여 시체에 스킬을 낭비하지 않게 합니다.
			if (IsValid(BaseChar) && BaseChar->IsDead()) continue;

			// 🟢 [핵심 수정 2] 중심 간 거리(D)에서 보스와 플레이어의 캡슐 반지름을 빼서 실제 닿는 거리를 구함
			float D = FVector::Dist2D(Boss->GetActorLocation(), PlayerPawn->GetActorLocation());

			float BossRadius = Boss->GetSimpleCollisionRadius();
			float PlayerRadius = PlayerPawn->GetSimpleCollisionRadius();

			// 이제 Z축 오차가 사라진 완벽한 '표면 간 거리'가 계산됩니다.
			float RealDist = D - BossRadius - PlayerRadius;

			// 가장 가까운 '표면 거리'를 가진 타겟을 선정
			if (RealDist < EdgeToEdgeDist)
			{
				EdgeToEdgeDist = RealDist;
				Target = PlayerPawn;
			}
		}
	}

	if (!IsValid(Target))
	{
		GetWorldTimerManager().SetTimer(ActionTimer, this, &AStage3BossAIController::DecideNextAction, 1.0f, false);
		return;
	}

	float AttackRange = BossData->AttackRange > 0.0f ? BossData->AttackRange : 300.0f;
	float Time = GetWorld()->GetTimeSeconds();
	TMap<EBoss3Skill, float> Weights;

	// ==============================================================================
	// 🟢 [핵심 수정 3] MinDist 대신 EdgeToEdgeDist를 사용하여 평타 사거리 이내인지 판정
	// ==============================================================================
	if (EdgeToEdgeDist <= AttackRange)
	{
		float BasicCooldown = BossData->Stage3SkillConfigs.Contains(EBoss3Skill::Basic) ? BossData->Stage3SkillConfigs[EBoss3Skill::Basic].Cooldown : 2.0f;
		float LastBasicTime = LastSkillUseTimes.Contains(EBoss3Skill::Basic) ? LastSkillUseTimes[EBoss3Skill::Basic] : -1000.0f;

		if (Time - LastBasicTime >= BasicCooldown)
		{
			float BasicWeight = BossData->Stage3SkillConfigs.Contains(EBoss3Skill::Basic) ? BossData->Stage3SkillConfigs[EBoss3Skill::Basic].Weight : 100.0f;
			Weights.Add(EBoss3Skill::Basic, BasicWeight);
		}
	}

	// ==============================================================================
	// 특수 스킬 쿨타임 체크
	// ==============================================================================
	for (const auto& Pair : BossData->Stage3SkillConfigs)
	{
		EBoss3Skill SkillType = Pair.Key;
		if (SkillType == EBoss3Skill::Basic) continue;

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
	// 🟢 [스킬 실행]
	// ==============================================================================
	float Duration = 1.0f;

	// 왜 이렇게 짰는지: [Data-Driven 최적화] 각 스킬별로 설정된 고유의 후딜레이(PostSkillDelay)를 가져옵니다.
	float PostSkillDelay = 1.5f; // 기본값
	if (BossData->Stage3SkillConfigs.Contains(Selected))
	{
		PostSkillDelay = BossData->Stage3SkillConfigs[Selected].PostSkillDelay;
	}

	// 기존 포커스(바라보기) 해제
	ClearFocus(EAIFocusPriority::Gameplay);

	switch (Selected)
	{
	case EBoss3Skill::Basic:
		StopMovement();
		// 왜 이렇게 짰는지: [비주얼/네트워크] SetActorRotation 대신 SetFocus를 쓰면 엔진 AI가 스무스하게 타겟을 향해 돕니다.
		SetFocus(Target, EAIFocusPriority::Gameplay);
		Duration = Boss->Execute_BasicAttack() + PostSkillDelay;
		LastSkillUseTimes.Add(EBoss3Skill::Basic, Time);
		break;

	case EBoss3Skill::Laser:
		StopMovement();
		// 왜 이렇게 짰는지: 보스 본체의 Tick()에서 RInterpTo를 사용해 기획된 속도로 직접 천천히 회전해야 하기 때문입니다.
		ClearFocus(EAIFocusPriority::Gameplay);
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
		// 🟢 [수정됨] 언리얼 네비메시의 조기 정차(브레이크) 오차를 무시하기 위해, 
		// 목표 거리를 AttackRange의 절반(50%)으로 팍 줄여서 확실하게 멱살을 잡으러 들어가게 만듭니다.
		MoveToActor(Target, AttackRange * 0.5f);
		Duration = 0.25f;
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
		GetWorldTimerManager().ClearTimer(ActionTimer);

		// 왜 이렇게 짰는지: [안전성] 콜스택이 깊어져 Stack Overflow가 발생하는 것을 막기 위해 비동기적(0.1초 후)으로 판단을 재호출합니다.
		GetWorldTimerManager().SetTimer(ActionTimer, this, &AStage3BossAIController::DecideNextAction, 0.1f, false);
	}
}