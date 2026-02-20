#include "BS_Switch.h"
#include "BS_Door.h"
#include "Net/UnrealNetwork.h"
#include "Project_Bang_Squad/PhysicsPad/JumpPad.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameStateBase.h"

ABS_Switch::ABS_Switch()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
    RootComponent = BaseMesh;

    HandleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandleMesh"));
    HandleMesh->SetupAttachment(BaseMesh);
    
    InteractSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractSphere"));
    InteractSphere->SetupAttachment(RootComponent);
    InteractSphere->InitSphereRadius(150.0f); 
    InteractSphere->SetCollisionProfileName(TEXT("Trigger"));

    InteractWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractWidget"));
    InteractWidget->SetupAttachment(RootComponent);
    InteractWidget->SetRelativeLocation(FVector(0.f, 0.f, 100.f)); 
    InteractWidget->SetWidgetSpace(EWidgetSpace::Screen); 
    InteractWidget->SetDrawSize(FVector2D(150.f, 50.f));
    InteractWidget->SetVisibility(false);
}

void ABS_Switch::BeginPlay()
{
    Super::BeginPlay();
    InteractSphere->OnComponentBeginOverlap.AddDynamic(this, &ABS_Switch::OnSphereBeginOverlap);
    InteractSphere->OnComponentEndOverlap.AddDynamic(this, &ABS_Switch::OnSphereEndOverlap);
    InitialHandleLocation = HandleMesh->GetRelativeLocation();
}

void ABS_Switch::Interact_Implementation(APawn* InstigatorPawn)
{
    if (!HasAuthority() || bIsActivated) return;

    bIsActivated = true;
    ActivationTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();

    // 스위치 작동 시 UI 숨김
    if (InteractWidget) InteractWidget->SetVisibility(false);
    
    if (TargetDoor)
    {
        TargetDoor->ExecuteAction(SelectedAction);
    }

    for (AJumpPad* Pad : TargetJumpPads)
    {
        if (Pad)
        {
            Pad->TriggerRise();
        }
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

void ABS_Switch::OnRep_IsActivated()
{
    if (bIsActivated && InteractWidget)
    {
        InteractWidget->SetVisibility(false);
    }
}

void ABS_Switch::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABS_Switch, bIsActivated);
    DOREPLIFETIME(ABS_Switch, ActivationTime);
}

void ABS_Switch::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (bIsActivated) return;

    APawn* Pawn = Cast<APawn>(OtherActor);
    if (Pawn && Pawn->IsLocallyControlled())
    {
        InteractWidget->SetVisibility(true);
    }
}

void ABS_Switch::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    APawn* Pawn = Cast<APawn>(OtherActor);
    if (Pawn && Pawn->IsLocallyControlled())
    {
        InteractWidget->SetVisibility(false);
    }
}