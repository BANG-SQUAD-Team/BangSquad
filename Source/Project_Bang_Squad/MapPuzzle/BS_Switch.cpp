#include "BS_Switch.h"
#include "BS_Door.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h"

ABS_Switch::ABS_Switch()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
    RootComponent = BaseMesh;

    HandleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandleMesh"));
    HandleMesh->SetupAttachment(BaseMesh);
}

void ABS_Switch::BeginPlay()
{
    Super::BeginPlay();
    InitialHandleLocation = HandleMesh->GetRelativeLocation();
}

void ABS_Switch::Interact_Implementation(APawn* InstigatorPawn)
{
    if (!HasAuthority() || bIsActivated) return;

    bIsActivated = true;
    ActivationTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();

    if (TargetDoor)
    {
        TargetDoor->ExecuteAction(SelectedAction);
    }
}

void ABS_Switch::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bIsActivated && ActivationTime > 0.f)
    {
        float CurrentTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
        float Alpha = FMath::Clamp((CurrentTime - ActivationTime) / 1.0f, 0.f, 1.f);

        FVector TargetLocation = InitialHandleLocation + FVector(0.f, 14.f, 0.f);
        HandleMesh->SetRelativeLocation(FMath::Lerp(InitialHandleLocation, TargetLocation, Alpha));
    }
}

void ABS_Switch::OnRep_IsActivated() {}

void ABS_Switch::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABS_Switch, bIsActivated);
    DOREPLIFETIME(ABS_Switch, ActivationTime);
}