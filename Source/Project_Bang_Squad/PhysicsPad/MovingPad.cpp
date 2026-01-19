#include "MovingPad.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

AMovingPad::AMovingPad()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	AActor::SetReplicateMovement(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	MoveTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("MoveTimeline"));
}

void AMovingPad::BeginPlay()
{
	Super::BeginPlay();

	StartLocation = GetActorLocation();

	if (HasAuthority() && MoveCurve)
	{
		FOnTimelineFloat ProgressFunction;
		ProgressFunction.BindUFunction(this, FName("HandleMoveProgress"));
		MoveTimeline->AddInterpFloat(MoveCurve, ProgressFunction);

		MoveTimeline->SetTimelineLengthMode(ETimelineLengthMode::TL_LastKeyFrame);
		MoveTimeline->SetLooping(true);
		MoveTimeline->Play();
	}
}

void AMovingPad::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority())
	{
		CurrentAlpha = FMath::FInterpTo(CurrentAlpha, ServerAlpha, DeltaTime, InterpSpeed);
		UpdateLocation(CurrentAlpha);
	}
}

void AMovingPad::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMovingPad, ServerAlpha);
}

void AMovingPad::OnRep_ServerAlpha()
{
}

void AMovingPad::HandleMoveProgress(float Value)
{
	ServerAlpha = Value;
	UpdateLocation(ServerAlpha);
}

void AMovingPad::UpdateLocation(float Value)
{
	FVector CurrentLocation = FMath::Lerp(StartLocation, StartLocation + EndLocation, Value);
	SetActorLocation(CurrentLocation);
}