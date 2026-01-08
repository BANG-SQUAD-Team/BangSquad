#include "EnemyNormal.h"

#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Animation/AnimMontage.h"

AEnemyNormal::AEnemyNormal()
{
	PrimaryActorTick.bCanEverTick = false;

	// 멀티에서 RPC가 의미 있으려면 액터 복제 켜는 게 기본
	bReplicates = true;
}

void AEnemyNormal::BeginPlay()
{
	Super::BeginPlay();

	AcquireTarget();

	if (TargetPawn.IsValid())
	{
		StartChase(TargetPawn.Get());
	}
}

void AEnemyNormal::AcquireTarget()
{
	// 지금은 싱글/테스트 기준: 0번 플레이어
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	TargetPawn = PlayerPawn;
}

void AEnemyNormal::StartChase(APawn* NewTarget)
{
	if (!NewTarget) return;

	TargetPawn = NewTarget;

	// 즉시 1회
	UpdateMoveTo();

	// 주기적으로 경로 갱신
	GetWorldTimerManager().ClearTimer(RepathTimer);
	GetWorldTimerManager().SetTimer(
		RepathTimer,
		this,
		&AEnemyNormal::UpdateMoveTo,
		RepathInterval,
		true
	);
}

void AEnemyNormal::StopChase()
{
	GetWorldTimerManager().ClearTimer(RepathTimer);

	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->StopMovement();
	}

	TargetPawn = nullptr;
}

bool AEnemyNormal::IsInAttackRange() const
{
	APawn* TP = TargetPawn.Get();
	if (!TP) return false;

	return FVector::Dist(GetActorLocation(), TP->GetActorLocation()) <= AttackRange;
}

void AEnemyNormal::UpdateMoveTo()
{
	APawn* TP = TargetPawn.Get();
	if (!TP)
	{
		StopChase();
		return;
	}

	const float Dist = FVector::Dist(GetActorLocation(), TP->GetActorLocation());
	if (Dist > StopChaseDistance)
	{
		StopChase();
		return;
	}

	// 공격 중에는 이동 갱신 멈춤 (슬라이딩 방지)
	if (bIsAttacking)
	{
		return;
	}

	// 공격 거리면 공격 시도
	if (IsInAttackRange())
	{
		if (AAIController* AIC = Cast<AAIController>(GetController()))
		{
			AIC->StopMovement();
		}

		// 서버에 공격 요청 (싱글이면 서버가 자기 자신)
		Server_TryAttack();
		return;
	}

	// 아직 멀면 추적 유지
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->MoveToActor(TP, AcceptanceRadius, true, true, true, 0, true);
	}
}

void AEnemyNormal::Server_TryAttack_Implementation()
{
	// 서버 권한에서만 신뢰
	if (!HasAuthority())
	{
		return;
	}

	// 쿨다운 체크
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastAttackTime < AttackCooldown)
	{
		return;
	}

	// 서버에서 거리 재검증
	if (!IsInAttackRange())
	{
		return;
	}

	// 몽타주 배열 유효성 체크 (nullptr 제외)
	TArray<int32> ValidIndices;
	ValidIndices.Reserve(AttackMontages.Num());

	for (int32 i = 0; i < AttackMontages.Num(); ++i)
	{
		if (AttackMontages[i] != nullptr)
		{
			ValidIndices.Add(i);
		}
	}

	if (ValidIndices.Num() == 0)
	{
		return;
	}

	const int32 Pick = ValidIndices[FMath::RandRange(0, ValidIndices.Num() - 1)];

	LastAttackTime = Now;
	bIsAttacking = true;

	// 모든 클라에서 같은 인덱스 몽타주 재생
	Multicast_PlayAttackMontage(Pick);

	// 공격 끝 타이머(선택된 몽타주 길이 기준)
	float Duration = 0.7f;
	if (AttackMontages[Pick])
	{
		Duration = AttackMontages[Pick]->GetPlayLength();
	}

	GetWorldTimerManager().ClearTimer(AttackEndTimer);
	GetWorldTimerManager().SetTimer(
		AttackEndTimer,
		this,
		&AEnemyNormal::EndAttack,
		Duration,
		false
	);
}

void AEnemyNormal::Multicast_PlayAttackMontage_Implementation(int32 MontageIndex)
{
	if (MontageIndex < 0 || MontageIndex >= AttackMontages.Num())
	{
		return;
	}

	UAnimMontage* MontageToPlay = AttackMontages[MontageIndex];
	if (!MontageToPlay) return;

	PlayAnimMontage(MontageToPlay);
}

void AEnemyNormal::EndAttack()
{
	bIsAttacking = false;
}
