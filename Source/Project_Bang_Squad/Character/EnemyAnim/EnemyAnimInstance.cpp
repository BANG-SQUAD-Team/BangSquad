#include "Project_Bang_Squad/Character/EnemyAnim/EnemyAnimInstance.h"
#include "GameFramework/Pawn.h"

void UEnemyAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// 초기 1회 캐싱
	CachedPawn = TryGetPawnOwner();
	Speed = 0.f;
}

void UEnemyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	APawn* OwnerPawn = CachedPawn.Get();
	if (!OwnerPawn)
	{
		OwnerPawn = TryGetPawnOwner();
		CachedPawn = OwnerPawn;
	}

	if (!OwnerPawn)
	{
		Speed = 0.f;
		return;
	}

	// 실제 이동 결과(velocity) 기반이 가장 안정적
	Speed = OwnerPawn->GetVelocity().Size();
}
