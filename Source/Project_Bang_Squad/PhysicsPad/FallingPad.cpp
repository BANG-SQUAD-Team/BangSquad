#include "FallingPad.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"

AFallingPad::AFallingPad()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    AActor::SetReplicateMovement(true); 

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    RootComponent = MeshComp;
    MeshComp->SetMobility(EComponentMobility::Movable);

    DetectionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DetectionBox"));
    DetectionBox->SetupAttachment(RootComponent);
    DetectionBox->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
    DetectionBox->SetCollisionProfileName(TEXT("Trigger"));
}

void AFallingPad::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority() && DetectionBox) 
    {
        DetectionBox->OnComponentBeginOverlap.AddDynamic(this, &AFallingPad::OnBoxOverlap);
    }
}

void AFallingPad::OnBoxOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (HasAuthority() && OtherActor && OtherActor->IsA(ACharacter::StaticClass()))
    {
        DetectionBox->SetGenerateOverlapEvents(false);
        GetWorldTimerManager().SetTimer(FallTimerHandle, this, &AFallingPad::StartFalling, FallDelay, false);
    }
}

void AFallingPad::StartFalling()
{
    Multicast_StartFalling();
}

void AFallingPad::Multicast_StartFalling_Implementation()
{
    if (MeshComp)
    {
        MeshComp->SetSimulatePhysics(true);
        MeshComp->WakeRigidBody(); 
    }
}