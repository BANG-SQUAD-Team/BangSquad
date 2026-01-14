// MidBossAIController.cpp

#include "MidBossAIController.h"
#include "Project_Bang_Squad/Character/Enemy/EnemyMidBoss.h" 
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

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

		// [중요] 클라이언트(Neutrals)와 팀원(Friendlies) 모두 감지하도록 설정 (멀티플레이어 필수)
		SightConfig->DetectionByAffiliation.bDetectEnemies = true;
		SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
		SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

		GetPerceptionComponent()->ConfigureSense(*SightConfig);
		GetPerceptionComponent()->SetDominantSense(SightConfig->GetSenseImplementation());
	}

	CurrentAIState = EMidBossAIState::Idle;
	PrimaryActorTick.bCanEverTick = true;
	bHasRoared = false;
}

void AMidBossAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	bHasRoared = false;

	// Perception 이벤트 연결
	if (GetPerceptionComponent())
	{
		GetPerceptionComponent()->OnTargetPerceptionUpdated.AddDynamic(this, &AMidBossAIController::OnTargetDetected);
	}

	// [NEW] 랜덤 타겟 변경 타이머 시작 (에디터 값 적용, 무한 반복)
	GetWorld()->GetTimerManager().SetTimer(
		AggroTimerHandle,
		this,
		&AMidBossAIController::UpdateRandomTarget,
		TargetChangeInterval,
		true
	);
}

void AMidBossAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 죽었거나 기믹 상태면 행동 정지
	if (CurrentAIState == EMidBossAIState::Dead || CurrentAIState == EMidBossAIState::Gimmick)
	{
		return;
	}

	// 타겟 유효성 검사 (쫓다가 사라지거나 죽었을 때)
	if (CurrentAIState == EMidBossAIState::Chase || CurrentAIState == EMidBossAIState::Attack)
	{
		if (!IsValid(TargetActor))
		{
			// 타겟을 놓치면 Idle로 돌아감 (포효 상태는 유지)
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
					// 공격 재생 (서버: 리턴값 받음 / 클라이언트: 멀티캐스트로 재생됨)
					float AnimDuration = MyPawn->PlayRandomAttack();
					float WaitTime = (AnimDuration > 0.f) ? AnimDuration : 1.5f;
					GetWorld()->GetTimerManager().SetTimer(StateTimerHandle, this, &AMidBossAIController::FinishAttack, WaitTime, false);
				}
			}
			else
			{
				// 타겟을 향해 이동
				MoveToActor(TargetActor, 50.0f);
			}
		}
		break;

	case EMidBossAIState::Attack:
		break;
	}
}

// 피격 시 호출
void AMidBossAIController::OnDamaged(AActor* Attacker)
{
	if (CurrentAIState == EMidBossAIState::Dead || CurrentAIState == EMidBossAIState::Gimmick) return;

	GetWorld()->GetTimerManager().ClearTimer(StateTimerHandle);
	StopMovement();

	// 1. 진짜 공격자(플레이어 폰) 찾기 - 투사체 예외 처리
	AActor* RealTarget = Attacker;
	if (Attacker)
	{
		if (!RealTarget->IsA(APawn::StaticClass()))
		{
			if (Attacker->GetInstigator()) RealTarget = Attacker->GetInstigator();
			else if (Attacker->GetOwner()) RealTarget = Attacker->GetOwner();
		}
	}

	// 2. 아직 전투 전이라면, 때린 사람을 타겟으로 잡고 포효 (최초 어그로)
	if (!bHasRoared && RealTarget)
	{
		TargetActor = RealTarget;
		bHasRoared = true;
	}
	// *참고: 이미 전투 중(bHasRoared == true)이라면 여기서 타겟을 바꾸지 않습니다.
	// 5초마다 도는 UpdateRandomTarget 타이머가 랜덤으로 타겟을 섞어줄 것입니다.

	// 3. 피격 상태 전환 (스턴 모션)
	CurrentAIState = EMidBossAIState::Hit;

	float StunDuration = 0.5f;
	auto* MyPawn = Cast<AEnemyMidBoss>(GetPawn());
	if (MyPawn)
	{
		// 피격 모션 (멀티캐스트로 재생됨)
		float AnimTime = MyPawn->PlayHitReactAnim();
		if (AnimTime > 0.f) StunDuration = AnimTime;
	}

	GetWorld()->GetTimerManager().SetTimer(StateTimerHandle, this, &AMidBossAIController::StartChasing, StunDuration, false);

	UE_LOG(LogTemp, Warning, TEXT("BOSS: Hit by [%s]. Current Target: [%s]"),
		*Attacker->GetName(),
		TargetActor ? *TargetActor->GetName() : TEXT("None"));
}

void AMidBossAIController::SetDeadState()
{
	CurrentAIState = EMidBossAIState::Dead;
	StopMovement();
	GetWorld()->GetTimerManager().ClearTimer(StateTimerHandle);

	// [중요] 죽으면 타겟 변경 타이머도 확실하게 꺼야 합니다.
	GetWorld()->GetTimerManager().ClearTimer(AggroTimerHandle);
}

void AMidBossAIController::OnTargetDetected(AActor* Actor, FAIStimulus Stimulus)
{
	if (CurrentAIState == EMidBossAIState::Dead) return;

	auto* PlayerCharacter = Cast<ACharacter>(Actor);
	// 플레이어가 아니면 무시 (몬스터끼리 싸움 방지)
	if (!PlayerCharacter) return;

	if (Stimulus.WasSuccessfullySensed())
	{
		if (CurrentAIState == EMidBossAIState::Idle)
		{
			TargetActor = PlayerCharacter;

			// 아직 포효 안 했으면 포효 시작
			if (!bHasRoared)
			{
				bHasRoared = true;
				CurrentAIState = EMidBossAIState::Notice;
				StopMovement();

				auto* MyPawn = Cast<AEnemyMidBoss>(GetPawn());
				if (MyPawn)
				{
					// 포효 모션 (멀티캐스트)
					float Duration = MyPawn->PlayAggroAnim();
					float WaitTime = (Duration > 0.f) ? Duration : 1.5f;
					GetWorld()->GetTimerManager().SetTimer(StateTimerHandle, this, &AMidBossAIController::StartChasing, WaitTime, false);
				}
				else
				{
					StartChasing();
				}
			}
			else
			{
				StartChasing();
			}
		}
	}
}

void AMidBossAIController::StartChasing()
{
	if (CurrentAIState == EMidBossAIState::Dead) return;

	CurrentAIState = EMidBossAIState::Chase;

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

// [NEW] 랜덤 타겟 변경 함수 구현
void AMidBossAIController::UpdateRandomTarget()
{
	// 죽었거나 기믹 중이면 패스
	if (CurrentAIState == EMidBossAIState::Dead || CurrentAIState == EMidBossAIState::Gimmick) return;

	// 1. 현재 Perception에 감지된 모든 액터 가져오기
	TArray<AActor*> PerceivedActors;
	GetPerceptionComponent()->GetKnownPerceivedActors(UAISenseConfig_Sight::StaticClass(), PerceivedActors);

	// 2. 플레이어(Character)만 골라내기
	TArray<AActor*> ValidTargets;
	for (AActor* Actor : PerceivedActors)
	{
		ACharacter* PlayerChar = Cast<ACharacter>(Actor);
		// 플레이어이고, 살아있는지 확인 (죽은 플레이어 쫓아가면 안됨)
		if (PlayerChar && IsValid(PlayerChar))
		{
			ValidTargets.Add(Actor);
		}
	}

	// 3. 랜덤 선택
	if (ValidTargets.Num() > 0)
	{
		int32 RandomIndex = FMath::RandRange(0, ValidTargets.Num() - 1);
		AActor* NewTarget = ValidTargets[RandomIndex];

		// 타겟이 실제로 바뀌었을 때만 반응
		if (TargetActor != NewTarget)
		{
			TargetActor = NewTarget;

			// 갑자기 방향을 틀면 더 자연스러우므로 잠깐 멈춤 명령
			StopMovement();

			// 로그로 확인
			UE_LOG(LogTemp, Warning, TEXT("BOSS: Random Target Switch -> [%s]"), *TargetActor->GetName());
		}
	}
}