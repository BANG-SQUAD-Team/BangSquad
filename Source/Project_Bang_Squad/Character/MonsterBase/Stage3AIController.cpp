#include "Project_Bang_Squad/Character/Monsterbase/Stage3AIController.h"
#include "Project_Bang_Squad/Character/Enemy/Stage3MidBoss.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Kismet/GameplayStatics.h"

AStage3AIController::AStage3AIController()
{
	PrimaryActorTick.bCanEverTick = true;
	bIsAttacking = false;
	bIsMoving = false; // [추가] 초기값 세팅
}

void AStage3AIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// 내가 조종할 녀석이 3스테이지 미드보스인지 확인!
	ControlledBoss = Cast<AStage3MidBoss>(InPawn);
}

void AStage3AIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 공격 중이거나 보스가 없으면 Tick 정지
	if (bIsAttacking || !ControlledBoss) return;

	CurrentTarget = FindNearestPlayer();

	// 타겟이 있는데, 아직 이동 중이 아니라면? -> 이동 페이즈 시작!
	if (CurrentTarget && !bIsMoving)
	{
		StartMovementPhase();
	}
}

// =======================================================
// [신규 함수] 2초간 뚜벅뚜벅 걸어가는 페이즈
// =======================================================
void AStage3AIController::StartMovementPhase()
{
	if (!CurrentTarget) return;

	bIsMoving = true;

	// 언리얼 AI 네비게이션 시스템을 이용해 타겟에게 걸어갑니다! (여유 거리 50)
	MoveToActor(CurrentTarget, 50.0f);

	// 2초 뒤에 걸음을 멈추고 공격을 판단(DecidePattern) 하도록 타이머 세팅!
	GetWorldTimerManager().SetTimer(MovementTimer, this, &AStage3AIController::DecidePattern, 2.0f, false);
}


void AStage3AIController::DecidePattern()
{
	if (!ControlledBoss || !CurrentTarget) return;

	bIsMoving = false;   // [추가] 이동 타이머 끝!
	bIsAttacking = true; // 공격 시작!

	StopMovement();      // [핵심] 가던 길을 우뚝 멈춰 섭니다.

	// 이 시점(2초 걸어온 후)의 거리를 계산합니다.
	float DistanceToTarget = FVector::Dist(ControlledBoss->GetActorLocation(), CurrentTarget->GetActorLocation());
	float AttackWaitTime = 0.0f;

	if (DistanceToTarget > RangedAttackDistance)
	{
		AttackWaitTime = ControlledBoss->Execute_RangedAttack(CurrentTarget);
	}
	else
	{
		AttackWaitTime = ControlledBoss->Execute_BasicAttack();
	}

	if (AttackWaitTime <= 0.0f) AttackWaitTime = 1.0f;

	GetWorldTimerManager().SetTimer(AttackWaitTimer, this, &AStage3AIController::ResetAttackState, AttackWaitTime + 0.5f, false);
}

void AStage3AIController::ResetAttackState()
{
	bIsAttacking = false;
}

AActor* AStage3AIController::FindNearestPlayer()
{
	TArray<AActor*> Players;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABaseCharacter::StaticClass(), Players);

	AActor* NearestPlayer = nullptr;
	float MinDistance = 999999.0f;

	for (AActor* Actor : Players)
	{
		if (ABaseCharacter* Player = Cast<ABaseCharacter>(Actor))
		{
			if (!Player->IsDead())
			{
				float Dist = FVector::Dist(ControlledBoss->GetActorLocation(), Player->GetActorLocation());
				if (Dist < MinDistance)
				{
					MinDistance = Dist;
					NearestPlayer = Player;
				}
			}
		}
	}
	return NearestPlayer;
}