#include "MovingPad.h"
#include "Components/StaticMeshComponent.h"

AMovingPad::AMovingPad()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

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

	StartRelativeLocation = Mesh->GetRelativeLocation();

	if (MoveCurve)
	{
		FOnTimelineFloat ProgressFunction;
		ProgressFunction.BindUFunction(this, FName("HandleMoveProgress"));
		MoveTimeline->AddInterpFloat(MoveCurve, ProgressFunction);

		MoveTimeline->SetTimelineLengthMode(ETimelineLengthMode::TL_LastKeyFrame);

		MoveTimeline->SetLooping(true);
		MoveTimeline->Play();
	}
}

void AMovingPad::HandleMoveProgress(float Value)
{
	FVector CurrentLocation = FMath::Lerp(StartRelativeLocation, StartRelativeLocation + EndLocation, Value);
	Mesh->SetRelativeLocation(CurrentLocation);
}