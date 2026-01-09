#include "JumpPad.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"

AJumpPad::AJumpPad()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Mesh->CanCharacterStepUpOn = ECB_Yes;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(Mesh);
	TriggerBox->SetRelativeLocation(FVector(0.f, 0.f, 35.f));
	TriggerBox->SetBoxExtent(FVector(55.f, 55.f, 20.f));
	TriggerBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	TriggerBox->SetGenerateOverlapEvents(true);

	BounceTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("BounceTimeline"));
}

void AJumpPad::BeginPlay()
{
	Super::BeginPlay();

	InitialMeshScale = Mesh->GetRelativeScale3D();

	if (BounceCurve)
	{
		FOnTimelineFloat ProgressFunction;
		ProgressFunction.BindUFunction(this, FName("HandleTimelineProgress"));
		BounceTimeline->AddInterpFloat(BounceCurve, ProgressFunction);

		FOnTimelineEvent FinishedFunction;
		FinishedFunction.BindUFunction(this, FName("OnTimelineFinished"));
		BounceTimeline->SetTimelineFinishedFunc(FinishedFunction);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("JumpPad: BounceCurve is missing in Blueprint!"));
	}

	if (HasAuthority())
	{
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AJumpPad::OnOverlapBegin);
	}
}

void AJumpPad::HandleTimelineProgress(float Value)
{
	float ScaleZ = InitialMeshScale.Z * FMath::Lerp(1.0f, 1.4f, Value);
	float ScaleX = InitialMeshScale.X * FMath::Lerp(1.0f, 0.85f, Value);
	float ScaleY = InitialMeshScale.Y * FMath::Lerp(1.0f, 0.85f, Value);
	Mesh->SetRelativeScale3D(FVector(ScaleX, ScaleY, ScaleZ));
}

void AJumpPad::OnTimelineFinished()
{
	Mesh->SetRelativeScale3D(InitialMeshScale);
}

void AJumpPad::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) return;

	UE_LOG(LogTemp, Warning, TEXT("JumpPad: Overlap detected with %s"), *OtherActor->GetName());

	if (ACharacter* Character = Cast<ACharacter>(OtherActor))
	{
		float PadTopZ = Mesh->GetComponentLocation().Z + (Mesh->Bounds.BoxExtent.Z);
		float PlayerFootZ = Character->GetActorLocation().Z - (Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

		UE_LOG(LogTemp, Log, TEXT("JumpPad: PlayerFootZ: %f, PadTopZ: %f"), PlayerFootZ, PadTopZ);

		if (PlayerFootZ >= (PadTopZ - SurfaceThreshold))
		{
			Character->LaunchCharacter(FVector(0.f, 0.f, LaunchStrength), false, true);
			UE_LOG(LogTemp, Warning, TEXT("JumpPad: Character Launched!"));

			Multicast_PlayBounceAnimation();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("JumpPad: Foot position too low for jump."));
		}
	}
}

void AJumpPad::Multicast_PlayBounceAnimation_Implementation()
{
	if (BounceTimeline)
	{
		BounceTimeline->PlayFromStart();
	}
}