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
	bHasRoared = false; // 초기화
}

void AMidBossAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	bHasRoared = false; // 빙의될 때 다시 초기화

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
			// 타겟을 놓치면 Idle로 가지만, bHasRoared는 초기화하지 않음 (다시 봐도 포효 안 함)
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
				// 계속 이동 명령 갱신
				MoveToActor(TargetActor, 50.0f);
			}
		}
		break;

	case EMidBossAIState::Attack:
		break;
	}
}

// MidBossAIController.cpp

void AMidBossAIController::OnDamaged(AActor* Attacker)
{
	if (CurrentAIState == EMidBossAIState::Dead || CurrentAIState == EMidBossAIState::Gimmick) return;

	GetWorld()->GetTimerManager().ClearTimer(StateTimerHandle);
	StopMovement();

	// [핵심 수정 부분]
	// 때린 놈(Attacker)이 '폰(캐릭터)'이 아니면(예: 투사체), 그 주인을 찾아서 타겟으로 삼는다.
	if (Attacker)
	{
		AActor* RealTarget = Attacker;

		// 만약 Attacker가 폰(캐릭터)이 아니라면? (투사체일 확률 높음)
		if (!RealTarget->IsA(APawn::StaticClass()))
		{
			// 투사체의 주인(Instigator)을 찾아본다
			if (Attacker->GetInstigator())
			{
				RealTarget = Attacker->GetInstigator();
			}
			// 주인이 없으면 Owner라도 찾아본다
			else if (Attacker->GetOwner())
			{
				RealTarget = Attacker->GetOwner();
			}
		}

		TargetActor = RealTarget;
		bHasRoared = true; // 맞았으니 전투 시작 간주
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

	UE_LOG(LogTemp, Warning, TEXT("BOSS: Hit by [%s] -> Targeting [%s]"),
		*Attacker->GetName(),
		TargetActor ? *TargetActor->GetName() : TEXT("None"));
}

// 사망 처리 함수
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

			// [핵심] 아직 포효한 적이 없다면 -> 포효(Notice)
			if (!bHasRoared)
			{
				bHasRoared = true; // 플래그 세움
				CurrentAIState = EMidBossAIState::Notice;
				StopMovement();

				auto* MyPawn = Cast<AEnemyMidBoss>(GetPawn());
				if (MyPawn)
				{
					float Duration = MyPawn->PlayAggroAnim();
					float WaitTime = (Duration > 0.f) ? Duration : 0.1f;
					GetWorld()->GetTimerManager().SetTimer(StateTimerHandle, this, &AMidBossAIController::StartChasing, WaitTime, false);
				}
				else
				{
					StartChasing();
				}
			}
			// 이미 포효해봤으면 -> 바로 추적(Chase)
			else
			{
				StartChasing();
			}
		}
	}
}

// 추적 재개 함수
void AMidBossAIController::StartChasing()
{
	if (CurrentAIState == EMidBossAIState::Dead) return;

	// 1. 상태 변경
	CurrentAIState = EMidBossAIState::Chase;

	UE_LOG(LogTemp, Warning, TEXT("BOSS: Hit Stun Ended or Aggro Finish! Resuming Chase."));

	// 2. [핵심] 멈췄던 발을 떼도록 강제로 이동 명령 다시 실행
	if (TargetActor)
	{
		MoveToActor(TargetActor, 50.0f);
	}
}

void AMidBossAIController::FinishAttack()
{
	if (CurrentAIState == EMidBossAIState::Dead) return;
	CurrentAIState = EMidBossAIState::Chase;
}