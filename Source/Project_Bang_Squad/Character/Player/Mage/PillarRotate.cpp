#include "PillarRotate.h"
#include "Components/SphereComponent.h"
#include "Components/ArrowComponent.h"
#include "Curves/CurveFloat.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h" // 서버 시간 참조용

APillarRotate::APillarRotate()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	HingeHelper = CreateDefaultSubobject<USphereComponent>(TEXT("HingeHelper"));
	HingeHelper->SetupAttachment(SceneRoot);
	HingeHelper->SetSphereRadius(20.0f);
	HingeHelper->bIsEditorOnly = true;

	DirectionHelper = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionHelper"));
	DirectionHelper->SetupAttachment(SceneRoot);
	DirectionHelper->bIsEditorOnly = true;

	PillarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PillarMesh"));
	PillarMesh->SetupAttachment(SceneRoot);
}

void APillarRotate::BeginPlay()
{
	Super::BeginPlay();
}

void APillarRotate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// 모든 클라이언트가 동일한 애니메이션 상태를 갖도록 복제 등록
	DOREPLIFETIME(APillarRotate, bIsFalling);
	DOREPLIFETIME(APillarRotate, StartTime);
	DOREPLIFETIME(APillarRotate, ReplicatedRotationAxis);
	DOREPLIFETIME(APillarRotate, ReplicatedInitialRotation);
	DOREPLIFETIME(APillarRotate, ReplicatedInitialLocation);
}

void APillarRotate::ProcessMageInput(FVector Direction)
{
	if (!HasAuthority()) return; // 서버에서만 판정
	if (bIsFalling || StartTime > 0.0f) return;

	if (Direction.Y > 0.4f)
	{
		// [핵심] 상호작용 시점의 월드 상태를 캡처하여 고정합니다.
		if (AGameStateBase* GS = GetWorld()->GetGameState())
		{
			StartTime = GS->GetServerWorldTimeSeconds();
		}

		ReplicatedInitialRotation = GetActorRotation();
		ReplicatedInitialLocation = GetActorLocation();

		if (DirectionHelper)
		{
			// DirectionHelper의 방향을 기준으로 회전축을 서버에서 확정합니다.
			ReplicatedRotationAxis = DirectionHelper->GetRightVector();
		}
		else
		{
			ReplicatedRotationAxis = GetActorRightVector();
		}

		ReplicatedRotationAxis = ReplicatedRotationAxis.GetSafeNormal();

		bIsFalling = true;
		SetActorTickEnabled(true);

		SetMageHighlight(false);
		if (PillarMesh)
		{
			PillarMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		}
	}
}

void APillarRotate::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 서버 시간이 복제되었다면 애니메이션 계산 시작
	if (StartTime > 0.f)
	{
		if (AGameStateBase* GS = GetWorld()->GetGameState())
		{
			float ServerTime = GS->GetServerWorldTimeSeconds();
			float ElapsedTime = ServerTime - StartTime;

			float Alpha = FMath::Clamp(ElapsedTime / FallDuration, 0.0f, 1.0f);

			float TargetAlpha = Alpha;
			if (FallCurve)
			{
				// 커브도 현재 경과 시간을 기준으로 샘플링합니다.
				TargetAlpha = FallCurve->GetFloatValue(ElapsedTime);
			}

			UpdateFallProgress(TargetAlpha);

			// 애니메이션이 완료되면 틱을 종료하여 성능 최적화
			if (ElapsedTime >= FallDuration)
			{
				SetActorTickEnabled(false);
			}
		}
	}
}

void APillarRotate::UpdateFallProgress(float Alpha)
{
	// 서버에서 복제된 축과 초기값을 사용하여 쿼터니언 회전 적용
	FQuat DeltaRotation = FQuat(ReplicatedRotationAxis, FMath::DegreesToRadians(MaxFallAngle * Alpha));
	SetActorRotation(DeltaRotation * ReplicatedInitialRotation.Quaternion());

	// 위치 보간
	FVector TargetLoc = ReplicatedInitialLocation + ReplicatedInitialRotation.RotateVector(FallLocationOffset);
	SetActorLocation(FMath::Lerp(ReplicatedInitialLocation, TargetLoc, Alpha));
}

void APillarRotate::OnRep_bIsFalling()
{
	if (bIsFalling)
	{
		SetActorTickEnabled(true);
		SetMageHighlight(false);
	}
}

void APillarRotate::SetMageHighlight(bool bActive)
{
	if (PillarMesh)
	{
		if (bIsFalling || StartTime > 0.0f)
		{
			PillarMesh->SetRenderCustomDepth(false);
			return;
		}
		PillarMesh->SetRenderCustomDepth(bActive);
		PillarMesh->SetCustomDepthStencilValue(bActive ? 250 : 0);
	}
}