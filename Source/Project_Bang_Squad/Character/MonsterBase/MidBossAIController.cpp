// MidBossAIController.cpp

#include "MidBossAIController.h"
// 사용자 경로 유지
#include "Project_Bang_Squad/Character/Enemy/EnemyMidBoss.h" 
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

AMidBossAIController::AMidBossAIController()
{
	// 1. Perception(감각) 컴포넌트 생성
	SetPerceptionComponent(*CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AI Perception")));

	// 2. 시야 설정
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("Sight Config"));
	if (SightConfig)
	{
		SightConfig->SightRadius = 1500.0f;
		SightConfig->LoseSightRadius = 2000.0f;
		SightConfig->PeripheralVisionAngleDegrees = 90.0f;
		SightConfig->DetectionByAffiliation.bDetectEnemies = true;
		SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
		SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
		GetPerceptionComponent()->ConfigureSense(*SightConfig);
		GetPerceptionComponent()->SetDominantSense(SightConfig->GetSenseImplementation());
	}

	CurrentAIState = EMidBossAIState::Idle;
	PrimaryActorTick.bCanEverTick = true;
}

void AMidBossAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// 폰에 빙의했을 때 Perception(감각) 시스템의 이벤트를 연결합니다.
	if (GetPerceptionComponent())
	{
		GetPerceptionComponent()->OnTargetPerceptionUpdated.AddDynamic(this, &AMidBossAIController::OnTargetDetected);
	}
}

void AMidBossAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 죽었거나 기믹 상태면 아무것도 안 함 (AI 정지)
	if (CurrentAIState == EMidBossAIState::Dead || CurrentAIState == EMidBossAIState::Gimmick)
	{
		return;
	}

	// 타겟 유효성 검사
	if (CurrentAIState == EMidBossAIState::Chase || CurrentAIState == EMidBossAIState::Attack)
	{
		if (!IsValid(TargetActor))
		{
			CurrentAIState = EMidBossAIState::Idle;
			StopMovement();
			return;
		}
	}

	switch (CurrentAIState)
	{
	case EMidBossAIState::Idle:
	case EMidBossAIState::Notice:
	case EMidBossAIState::Hit:
	case EMidBossAIState::Dead:
		break;

	case EMidBossAIState::Chase:
		if (TargetActor)
		{
			float Distance = GetPawn()->GetDistanceTo(TargetActor);
			if (Distance <= AttackRange)
			{
				StopMovement();
				CurrentAIState = EMidBossAIState::Attack;

				auto* MyPawn = Cast<AEnemyMidBoss>(GetPawn());
				if (MyPawn)
				{
					float AnimDuration = MyPawn->PlayRandomAttack();
					float WaitTime = (AnimDuration > 0.f) ? AnimDuration : 0.5f;
					GetWorld()->GetTimerManager().SetTimer(StateTimerHandle, this, &AMidBossAIController::FinishAttack, WaitTime, false);
				}
			}
			else
			{
				MoveToActor(TargetActor, 50.0f);
			}
		}
		break;

	case EMidBossAIState::Attack:
		break;
	}
}

void AMidBossAIController::OnDamaged(AActor* Attacker)
{
	// 죽었거나 기믹 상태면 무시
	if (CurrentAIState == EMidBossAIState::Dead || CurrentAIState == EMidBossAIState::Gimmick) return;

	GetWorld()->GetTimerManager().ClearTimer(StateTimerHandle);
	StopMovement();

	if (Attacker)
	{
		TargetActor = Attacker;
	}

	CurrentAIState = EMidBossAIState::Hit;

	float StunDuration = 0.5f;
	auto* MyPawn = Cast<AEnemyMidBoss>(GetPawn());
	if (MyPawn)
	{
		float AnimTime = MyPawn->PlayHitReactAnim();
		if (AnimTime > 0.f) StunDuration = AnimTime;
	}

	GetWorld()->GetTimerManager().SetTimer(StateTimerHandle, this, &AMidBossAIController::StartChasing, StunDuration, false);

	UE_LOG(LogTemp, Warning, TEXT("BOSS: Ouch! Hit Reaction -> Will Chase soon."));
}

// [필수 추가] 사망 처리 함수
void AMidBossAIController::SetDeadState()
{
	CurrentAIState = EMidBossAIState::Dead;
	StopMovement();
	GetWorld()->GetTimerManager().ClearTimer(StateTimerHandle);
}

void AMidBossAIController::OnTargetDetected(AActor* Actor, FAIStimulus Stimulus)
{
	if (CurrentAIState == EMidBossAIState::Dead) return;

	auto* PlayerCharacter = Cast<ACharacter>(Actor);
	if (!PlayerCharacter || !PlayerCharacter->IsPlayerControlled()) return;

	if (Stimulus.WasSuccessfullySensed())
	{
		if (CurrentAIState == EMidBossAIState::Idle)
		{
			TargetActor = PlayerCharacter;
			CurrentAIState = EMidBossAIState::Notice;
			StopMovement();

			auto* MyPawn = Cast<AEnemyMidBoss>(GetPawn());
			if (MyPawn)
			{
				float Duration = MyPawn->PlayAggroAnim();
				float WaitTime = (Duration > 0.f) ? Duration : 0.1f;
				GetWorld()->GetTimerManager().SetTimer(StateTimerHandle, this, &AMidBossAIController::StartChasing, WaitTime, false);
			}
		}
	}
}

void AMidBossAIController::StartChasing()
{
	if (CurrentAIState == EMidBossAIState::Dead) return;
	CurrentAIState = EMidBossAIState::Chase;
}

void AMidBossAIController::FinishAttack()
{
	if (CurrentAIState == EMidBossAIState::Dead) return;
	CurrentAIState = EMidBossAIState::Chase;
}