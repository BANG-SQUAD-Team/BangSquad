#include "Project_Bang_Squad/Character/StageBoss/BossLiftPillar.h"
#include "TimerManager.h"
#include "Engine/World.h"

ABossLiftPillar::ABossLiftPillar()
{
	bReplicates = true;
	SetReplicateMovement(true);

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;

	MeshComp->SetCollisionProfileName(TEXT("BlockAll"));
	MeshComp->SetSimulatePhysics(false);
	MeshComp->SetGenerateOverlapEvents(false);
	MeshComp->SetUseCCD(true); // 보스 밀어내기 강화

	LiftMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("LiftMovement"));
	LiftMovement->UpdatedComponent = RootComponent;
	LiftMovement->ProjectileGravityScale = 0.f;
	LiftMovement->bShouldBounce = false;
	LiftMovement->InitialSpeed = 0.f;
	LiftMovement->MaxSpeed = 10000.f;

	// 보스 강제 부양을 위한 핵심 설정
	LiftMovement->bSweepCollision = false;
}

void ABossLiftPillar::InitializeLift(float TargetHeight, float RiseDuration, float LifeTimeOffset)
{
	if (!HasAuthority()) return;

	if (RiseDuration <= 0.f) RiseDuration = 0.1f;

	float RiseSpeed = TargetHeight / RiseDuration;
	LiftMovement->Velocity = FVector::UpVector * RiseSpeed;

	FTimerHandle StopTimerHandle;
	GetWorldTimerManager().SetTimer(StopTimerHandle, [this]()
		{
			if (IsValid(this) && IsValid(LiftMovement))
			{
				LiftMovement->StopMovementImmediately();
				LiftMovement->Deactivate();
			}
		}, RiseDuration, false);

	// [수정] 상승 시간 + 설정한 유지 시간만큼 생존
	SetLifeSpan(RiseDuration + LifeTimeOffset);
}