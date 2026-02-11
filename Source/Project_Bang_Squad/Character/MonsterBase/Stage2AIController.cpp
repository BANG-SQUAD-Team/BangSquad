// Source/Project_Bang_Squad/Character/MonsterBase/Stage2AIController.cpp

#include "Project_Bang_Squad/Character/MonsterBase/Stage2AIController.h"
#include "Project_Bang_Squad/Character/Enemy/Stage2MidBoss.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h" // 필수 헤더 추가
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"
void AStage2AIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	MyPawn = Cast<AStage2MidBoss>(InPawn);

	CurrentState = EBossPatternState::RangedBarrage;
	RangedAttackCount = 0;
	bIsBusy = false;

	// 시작 시 타겟 찾기
	FindNewTarget();
}

void AStage2AIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!MyPawn || !HasAuthority()) return;

	// 타겟 바라보기 (공격 중이 아닐 때만 or 원거리 공격 준비 중일 때)
	if (CurrentTarget && !bIsBusy)
	{
		FVector Dir = CurrentTarget->GetActorLocation() - MyPawn->GetActorLocation();
		Dir.Z = 0.f;
		MyPawn->SetActorRotation(FMath::RInterpTo(MyPawn->GetActorRotation(), Dir.Rotation(), DeltaTime, 5.0f));
	}

	RunPatternLogic();
}

void AStage2AIController::RunPatternLogic()
{
	// 행동 중이거나 타겟이 없으면 패스
	if (bIsBusy || !CurrentTarget)
	{
		if (!CurrentTarget) FindNewTarget();
		return;
	}

	float Duration = 1.0f; // 기본 대기 시간

	switch (CurrentState)
	{
	case EBossPatternState::RangedBarrage:
		// [원거리 공격 3회]
		if (RangedAttackCount < 3)
		{
			// 공격 실행
			Duration = MyPawn->Execute_RangedAttack(CurrentTarget);

			// 다음 발사 준비
			RangedAttackCount++;

			// 몽타주 길이보다 조금 더 기다려서 자연스럽게 연결
			if (Duration <= 0.f) Duration = 1.0f;
		}
		else
		{
			// 3회 완료 -> 텔레포트 상태로 전환
			CurrentState = EBossPatternState::Teleporting;
			Duration = 0.1f; // 즉시 전환
		}
		break;

	case EBossPatternState::Teleporting:
		// [텔레포트]
		MyPawn->Execute_TeleportToTarget(CurrentTarget);

		// 텔레포트 후 즉시 공격 태세
		CurrentState = EBossPatternState::MeleeSmash;
		Duration = 0.5f; // 나타나는 연출 시간 확보
		break;

	case EBossPatternState::MeleeSmash:
		// [근접 공격]
		Duration = MyPawn->Execute_MeleeAttack();

		// 공격 후 대기 상태로
		CurrentState = EBossPatternState::Waiting;
		if (Duration <= 0.f) Duration = 1.0f;
		break;

	case EBossPatternState::Waiting:
		// [어그로 변경 및 초기화]
		FindNewTarget(); // 타겟 변경

		// 리셋
		CurrentState = EBossPatternState::RangedBarrage;
		RangedAttackCount = 0;
		Duration = 1.0f; // 다음 패턴 시작 전 잠깐 휴식
		break;
	}

	// 행동 시작 (Busy 설정)
	bIsBusy = true;
	GetWorld()->GetTimerManager().SetTimer(ActionTimer, this, &AStage2AIController::OnActionFinished, Duration, false);
}

void AStage2AIController::OnActionFinished()
{
	// 행동 끝, 다음 Tick에서 로직 수행 가능
	bIsBusy = false;
}

void AStage2AIController::FindNewTarget()
{
	TArray<AActor*> AlivePlayers;

	// 접속 중인 모든 플레이어 컨트롤러 순회 (NPC/몬스터 제외)
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->GetPawn())
		{
			APawn* PlayerPawn = PC->GetPawn();

			// [수정] IsTargetDead 대신 직접 HealthComponent를 가져와서 생존 여부 확인
			if (UHealthComponent* HP = PlayerPawn->FindComponentByClass<UHealthComponent>())
			{
				if (!HP->IsDead()) // 체력이 0이 아니면
				{
					AlivePlayers.Add(PlayerPawn);
				}
			}
			// 만약 HealthComponent가 없는 캐릭터라면(예외), 그냥 Alive로 간주하거나 제외
			else
			{
				AlivePlayers.Add(PlayerPawn);
			}
		}
	}

	// 살아있는 플레이어 중에서 랜덤 선택
	if (AlivePlayers.Num() > 0)
	{
		int32 RandIdx = FMath::RandRange(0, AlivePlayers.Num() - 1);
		CurrentTarget = AlivePlayers[RandIdx];
	}
	else
	{
		// 살아있는 플레이어가 없으면 타겟 해제
		CurrentTarget = nullptr;
	}
}