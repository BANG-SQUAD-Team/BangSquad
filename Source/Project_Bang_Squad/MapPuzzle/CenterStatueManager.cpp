#include "Project_Bang_Squad/MapPuzzle/CenterStatueManager.h"
#include "Project_Bang_Squad/Character/Enemy/EnemySpawner.h" 
#include "Project_Bang_Squad/MapPuzzle/Stage3PuzzleManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ArrowComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Curves/CurveFloat.h" 

ACenterStatueManager::ACenterStatueManager()
{
	PrimaryActorTick.bCanEverTick = true; // Tick 필수
	bReplicates = true;

	// 1. 루트 생성 (움직이지 않는 기준점)
	DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	RootComponent = DefaultSceneRoot;

	// 2. 화살표 (방향 지정용)
	FallDirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("FallDirectionArrow"));
	FallDirectionArrow->SetupAttachment(RootComponent);
	FallDirectionArrow->SetRelativeLocation(FVector(0, 0, 100));
	FallDirectionArrow->ArrowSize = 3.0f;

	// 3. 석상 (루트에 붙음 -> 나중에 혼자 움직임)
	StatueMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatueMesh"));
	StatueMesh->SetupAttachment(RootComponent);
	StatueMesh->SetMobility(EComponentMobility::Movable);

	// 4. 불꽃들 (루트에 붙음 -> 석상과 형제 관계 -> 석상 움직임 영향 안 받음)
	LeftFireMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftFireMesh"));
	LeftFireMesh->SetupAttachment(RootComponent);
	LeftFireMesh->SetHiddenInGame(true);

	RightFireMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightFireMesh"));
	RightFireMesh->SetupAttachment(RootComponent);
	RightFireMesh->SetHiddenInGame(true);
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

	// [회전값 계산] 화살표 방향으로 90도 눕기
	if (StatueMesh)
	{
		StartQuat = StatueMesh->GetRelativeRotation().Quaternion();

		if (FallDirectionArrow)
		{
			// 화살표가 가리키는 방향
			FVector FallDir = FallDirectionArrow->GetForwardVector();

			// 회전축 계산 (Up 벡터와 외적)
			FVector RotationAxis = FVector::CrossProduct(FVector::UpVector, FallDir).GetSafeNormal();

			// 90도 회전 쿼터니언
			FQuat RotationDelta = FQuat(RotationAxis, FMath::DegreesToRadians(90.0f));

			// 최종 목표 회전값
			EndQuat = RotationDelta * StartQuat;
		}
	}

	// 커브 총 길이 계산
	if (FallCurve)
	{
		float MinTime, MaxTime;
		FallCurve->GetTimeRange(MinTime, MaxTime);
		MaxCurveTime = MaxTime;
	}

	if (HasAuthority())
	{
		if (BossSpawner)
		{
			BossSpawner->SetActorEnableCollision(false);
			BossSpawner->OnSpawnerCleared.AddDynamic(this, &ACenterStatueManager::OnBossDefeated);
		}
	}
}

// [핵심] Tick에서 직접 커브를 읽어 움직임
void ACenterStatueManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsFalling && FallCurve && StatueMesh)
	{
		// 1. 시간 흐름
		CurrentCurveTime += DeltaTime;

		// 2. 커브 값 가져오기 (0.0 ~ 1.0)
		float Value = FallCurve->GetFloatValue(CurrentCurveTime);

		// 3. 회전 보간 (부드럽게 눕기)
		FQuat NewQuat = FQuat::Slerp(StartQuat, EndQuat, Value);
		StatueMesh->SetRelativeRotation(NewQuat);

		// 4. 종료 체크
		if (CurrentCurveTime >= MaxCurveTime)
		{
			bIsFalling = false;
			OnFallFinished();
		}
	}
}

void ACenterStatueManager::ActivateLeftGoblet() { if (!HasAuthority() || bLeftActive) return; bLeftActive = true; OnRep_LeftActive(); CheckPuzzleCompletion(); }
void ACenterStatueManager::ActivateRightGoblet() { if (!HasAuthority() || bRightActive) return; bRightActive = true; OnRep_RightActive(); CheckPuzzleCompletion(); }
void ACenterStatueManager::OnRep_LeftActive() { if (LeftFireMesh) LeftFireMesh->SetHiddenInGame(false); }
void ACenterStatueManager::OnRep_RightActive() { if (RightFireMesh) RightFireMesh->SetHiddenInGame(false); }
void ACenterStatueManager::CheckPuzzleCompletion() { if (bLeftActive && bRightActive && !bPuzzleCompleted) { bPuzzleCompleted = true; if (BossSpawner) BossSpawner->SetActorEnableCollision(true); } }


// --- [보스 처치 및 낙하 로직] ---

void ACenterStatueManager::OnBossDefeated()
{
	if (!HasAuthority()) return;
	Multicast_StartFallSequence();
}

void ACenterStatueManager::Multicast_StartFallSequence_Implementation()
{
	// Falling 시작 트리거
	bIsFalling = true;
	CurrentCurveTime = 0.0f;
}

void ACenterStatueManager::OnFallFinished()
{
	if (StatueMesh)
	{
		// 1. 석상 본체 물리 켜고 충돌 끄기
		StatueMesh->SetSimulatePhysics(true);
		StatueMesh->SetCollisionResponseToAllChannels(ECR_Ignore);

		// 2. [중요] 자식 컴포넌트(도끼 등)도 찾아서 충돌 끄기
		TArray<USceneComponent*> ChildrenComponents;
		StatueMesh->GetChildrenComponents(true, ChildrenComponents); // true = 자손 모두 포함

		for (USceneComponent* Child : ChildrenComponents)
		{
			if (UPrimitiveComponent* PrimChild = Cast<UPrimitiveComponent>(Child))
			{
				// 도끼도 바닥을 뚫고 떨어져야 하므로 충돌 무시
				PrimChild->SetCollisionResponseToAllChannels(ECR_Ignore);
			}
		}
	}

	if (HasAuthority())
	{
		if (PuzzleManager)
		{
			PuzzleManager->StartSequence();
		}

		GetWorldTimerManager().SetTimer(DestroyTimerHandle, this, &ACenterStatueManager::StartDestroyTimer, 10.0f, false);
	}
}

void ACenterStatueManager::StartDestroyTimer()
{
	// 서버에서 타이머가 끝나면 멀티캐스트로 모두에게 알림
	Multicast_DestroyStatueMesh();
}

void ACenterStatueManager::Multicast_DestroyStatueMesh_Implementation()
{
	if (StatueMesh)
	{
		StatueMesh->DestroyComponent();
		StatueMesh = nullptr;
	}
}