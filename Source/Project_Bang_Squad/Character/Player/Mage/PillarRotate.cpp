#include "PillarRotate.h"
#include "Components/SphereComponent.h"
#include "Components/ArrowComponent.h"
#include "Curves/CurveFloat.h"
#include "Net/UnrealNetwork.h"

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
	HingeHelper->ShapeColor = FColor::Magenta;
	HingeHelper->bIsEditorOnly = true;

	DirectionHelper = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionHelper"));
	DirectionHelper->SetupAttachment(SceneRoot);
	DirectionHelper->ArrowColor = FColor::Cyan;
	DirectionHelper->bIsEditorOnly = true;

	PillarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PillarMesh"));
	PillarMesh->SetupAttachment(SceneRoot);
}

void APillarRotate::BeginPlay()
{
	Super::BeginPlay();
	InitialRotation = GetActorRotation();
	InitialLocation = GetActorLocation();

	if (DirectionHelper)
	{
		RotationAxis = DirectionHelper->GetRightVector();
	}
}

void APillarRotate::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsFalling)
	{
		CurrentTime += DeltaTime;

		float Alpha = FMath::Clamp(CurrentTime / FallDuration, 0.0f, 1.0f);

		float TargetAlpha = Alpha;
		if (FallCurve)
		{
			TargetAlpha = FallCurve->GetFloatValue(CurrentTime);
		}

		UpdateFallProgress(TargetAlpha);

		if (CurrentTime >= FallDuration)
		{
			UpdateFallProgress(FallCurve ? FallCurve->GetFloatValue(FallDuration) : 1.0f);
			bIsFalling = false;
			SetActorTickEnabled(false);
		}
	}
}

void APillarRotate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APillarRotate, bIsFalling);
}

void APillarRotate::SetMageHighlight(bool bActive)
{
	if (PillarMesh)
	{
		if (bIsFalling || CurrentTime > 0.0f)
		{
			PillarMesh->SetRenderCustomDepth(false);
			return;
		}

		PillarMesh->SetRenderCustomDepth(bActive);
		PillarMesh->SetCustomDepthStencilValue(bActive ? 250 : 0);
	}
}

void APillarRotate::ProcessMageInput(FVector Direction)
{
	if (!HasAuthority()) return;
	if (bIsFalling || CurrentTime > 0.0f) return;

	if (Direction.Y > 0.4f)
	{
		bIsFalling = true;
		SetActorTickEnabled(true);
		
		SetMageHighlight(false);
		if (PillarMesh)
		{
			PillarMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		}
	}
}

void APillarRotate::OnRep_bIsFalling()
{
	if (bIsFalling)
	{
		SetActorTickEnabled(true);
		SetMageHighlight(false);
		if (PillarMesh)
		{
			PillarMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		}
	}
}

void APillarRotate::UpdateFallProgress(float Alpha)
{
	FQuat DeltaRotation = FQuat(RotationAxis, FMath::DegreesToRadians(MaxFallAngle * Alpha));
	SetActorRotation(DeltaRotation * InitialRotation.Quaternion());

	FVector TargetLoc = InitialLocation + InitialRotation.RotateVector(FallLocationOffset);
	SetActorLocation(FMath::Lerp(InitialLocation, TargetLoc, Alpha));
}