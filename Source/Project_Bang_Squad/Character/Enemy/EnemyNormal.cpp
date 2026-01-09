#include "EnemyNormal.h"

#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"

AEnemyNormal::AEnemyNormal()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true; // RPC 의미 있으려면 기본
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

	UpdateMoveTo();

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

inline void AEnemyNormal::UpdateMoveTo()


{
	if (IsDead()) return;

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

bool AEnemyNormal::IsInAttackRange() const
{
	APawn* TP = TargetPawn.Get();
	if (!TP) return false;

	return FVector::Dist(GetActorLocation(), TP->GetActorLocation()) <= AttackRange;
}

void AEnemyNormal::Server_TryAttack_Implementation()
{
	if (IsDead()) return;

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

	// 공격 1회 시작 시, "이번 공격에서 데미지 1번만" 허용하도록 초기화
	bDamageAppliedThisAttack = false;

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

// AnimNotify에서 호출될 타격 처리 (서버만 데미지 적용)
inline void AEnemyNormal::AnimNotify_AttackHit()
{
	// 데미지는 서버만!
	if (!HasAuthority()) return;

	// 공격 1회당 데미지 1번
	if (bDamageAppliedThisAttack) return;

	APawn* Victim = TargetPawn.Get();
	if (!IsValid(Victim)) return;

	// 타격 순간에도 거리 재검증 (애니만 보고 맞았다고 치면 판정이 구려짐)
	if (!IsInAttackRange()) return;

	bDamageAppliedThisAttack = true;

	UGameplayStatics::ApplyDamage(
		Victim,
		AttackDamage,
		GetController(),
		this,
		UDamageType::StaticClass()
	);
}

void AEnemyNormal::OnDeathStarted()
{
	// 죽으면 더 이상 추적/공격 관련 타이머가 돌 필요가 없음 (버그/낭비 차단)
	GetWorldTimerManager().ClearTimer(RepathTimer);
	GetWorldTimerManager().ClearTimer(AttackEndTimer);

	// 추적 상태도 정리 (TargetPawn 비우고 이동 중지)
	StopChase();

	// 공격 중이었다면 상태도 종료
	bIsAttacking = false;
}

