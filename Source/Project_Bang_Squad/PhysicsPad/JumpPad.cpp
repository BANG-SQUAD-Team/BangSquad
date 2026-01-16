#include "JumpPad.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"

AJumpPad::AJumpPad()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true; // 액터 복제 활성화
	AActor::SetReplicateMovement(true); // 이동 및 스케일 변화 복제

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

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

	// 서버에서만 오버랩 이벤트를바인딩하여 판정 독점
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

	if (ACharacter* Character = Cast<ACharacter>(OtherActor))
	{
		float PadTopZ = Mesh->GetComponentLocation().Z + (Mesh->Bounds.BoxExtent.Z);
		float PlayerFootZ = Character->GetActorLocation().Z - (Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

		if (PlayerFootZ >= (PadTopZ - SurfaceThreshold))
		{
			Character->LaunchCharacter(FVector(0.f, 0.f, LaunchStrength), false, true);
			Multicast_PlayBounceAnimation(); 
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