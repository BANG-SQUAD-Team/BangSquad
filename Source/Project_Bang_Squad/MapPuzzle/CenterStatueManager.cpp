#include "Project_Bang_Squad/MapPuzzle/CenterStatueManager.h"
#include "Project_Bang_Squad/Character/Enemy/EnemySpawner.h" 
#include "Components/StaticMeshComponent.h"
#include "Components/TimelineComponent.h"
#include "Components/ArrowComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ACenterStatueManager::ACenterStatueManager()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// 1. 루트 (고정점)
	DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	RootComponent = DefaultSceneRoot;

	// 2. 화살표 (넘어질 방향 지정용)
	FallDirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("FallDirectionArrow"));
	FallDirectionArrow->SetupAttachment(RootComponent);
	FallDirectionArrow->SetRelativeLocation(FVector(0, 0, 100)); // 잘 보이게 위로
	FallDirectionArrow->ArrowSize = 3.0f; // 큼직하게

	// 3. 석상 (움직일 놈)
	StatueMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatueMesh"));
	StatueMesh->SetupAttachment(RootComponent);
	StatueMesh->SetMobility(EComponentMobility::Movable);

	// 4. 불꽃들 (루트에 붙여서 석상과 분리!)
	LeftFireMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftFireMesh"));
	LeftFireMesh->SetupAttachment(RootComponent); // <-- 중요: 루트에 붙임
	LeftFireMesh->SetHiddenInGame(true);

	RightFireMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightFireMesh"));
	RightFireMesh->SetupAttachment(RootComponent); // <-- 중요: 루트에 붙임
	RightFireMesh->SetHiddenInGame(true);

	FallTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("FallTimeline"));
	FallTimeline->SetIsReplicated(true);
}

void ACenterStatueManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACenterStatueManager, bLeftActive);
	DOREPLIFETIME(ACenterStatueManager, bRightActive);
}

void ACenterStatueManager::BeginPlay()
{
	Super::BeginPlay();

	// [시작 회전값 저장]
	if (StatueMesh)
	{
		StartQuat = StatueMesh->GetRelativeRotation().Quaternion();

		// [목표 회전값 계산] 화살표 방향으로 90도 눕기
		if (FallDirectionArrow)
		{
			// 화살표가 가리키는 방향 (Forward)
			FVector FallDir = FallDirectionArrow->GetForwardVector();

			// 회전축: (위쪽 벡터) x (넘어질 방향) 의 외적 = 옆방향 축
			FVector RotationAxis = FVector::CrossProduct(FVector::UpVector, FallDir).GetSafeNormal();

			// 해당 축을 기준으로 90도 회전하는 쿼터니언 생성
			FQuat RotationDelta = FQuat(RotationAxis, FMath::DegreesToRadians(90.0f));

			// 목표 = (회전) * (원래 회전)
			EndQuat = RotationDelta * StartQuat;
		}
	}

	if (HasAuthority())
	{
		if (BossSpawner)
		{
			BossSpawner->SetActorEnableCollision(false);
			BossSpawner->OnSpawnerCleared.AddDynamic(this, &ACenterStatueManager::OnBossDefeated);
		}
	}

	if (FallCurve)
	{
		FOnTimelineFloat ProgressFunction;
		ProgressFunction.BindDynamic(this, &ACenterStatueManager::HandleFallProgress);
		FallTimeline->AddInterpFloat(FallCurve, ProgressFunction);

		FOnTimelineEvent FinishFunction;
		FinishFunction.BindDynamic(this, &ACenterStatueManager::OnFallFinished);
		FallTimeline->SetTimelineFinishedFunc(FinishFunction);
	}
}

void ACenterStatueManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (FallTimeline) FallTimeline->TickComponent(DeltaTime, ELevelTick::LEVELTICK_TimeOnly, NULL);
}

// ... (ActivateLeft/RightGoblet, OnRep 등 기존 함수들 그대로 유지) ...
void ACenterStatueManager::ActivateLeftGoblet() { if (!HasAuthority() || bLeftActive) return; bLeftActive = true; OnRep_LeftActive(); CheckPuzzleCompletion(); }
void ACenterStatueManager::ActivateRightGoblet() { if (!HasAuthority() || bRightActive) return; bRightActive = true; OnRep_RightActive(); CheckPuzzleCompletion(); }
void ACenterStatueManager::OnRep_LeftActive() { if (LeftFireMesh) LeftFireMesh->SetHiddenInGame(false); }
void ACenterStatueManager::OnRep_RightActive() { if (RightFireMesh) RightFireMesh->SetHiddenInGame(false); }
void ACenterStatueManager::CheckPuzzleCompletion() { if (bLeftActive && bRightActive && !bPuzzleCompleted) { bPuzzleCompleted = true; if (BossSpawner) BossSpawner->SetActorEnableCollision(true); } }


// --- [보스 처치 후 로직] ---

void ACenterStatueManager::OnBossDefeated()
{
	if (!HasAuthority()) return;
	Multicast_StartFallSequence();
}

void ACenterStatueManager::Multicast_StartFallSequence_Implementation()
{
	if (FallTimeline) FallTimeline->PlayFromStart();
}

void ACenterStatueManager::HandleFallProgress(float Value)
{
	if (!StatueMesh) return;

	// [중요] 쿼터니언 보간 (Slerp)으로 부드럽게 회전
	FQuat NewQuat = FQuat::Slerp(StartQuat, EndQuat, Value);
	StatueMesh->SetRelativeRotation(NewQuat);
}

void ACenterStatueManager::OnFallFinished()
{
	if (StatueMesh)
	{
		StatueMesh->SetSimulatePhysics(true);

		// 바닥 충돌 무시 (용암으로 추락)
		StatueMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
		StatueMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}

	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(DestroyTimerHandle, this, &ACenterStatueManager::DestroyStatue, 10.0f, false);
	}
}

void ACenterStatueManager::DestroyStatue()
{
	Destroy();
}