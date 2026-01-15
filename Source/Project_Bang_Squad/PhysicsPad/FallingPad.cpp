#include "FallingPad.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"

AFallingPad::AFallingPad()
{
    PrimaryActorTick.bCanEverTick = false;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    RootComponent = MeshComp;
    MeshComp->SetSimulatePhysics(false);
    MeshComp->SetMobility(EComponentMobility::Movable); // 코드로 Movable 강제 설정

    DetectionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DetectionBox"));
    DetectionBox->SetupAttachment(RootComponent);

    DetectionBox->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
    DetectionBox->SetBoxExtent(FVector(50.0f, 50.0f, 20.0f));

    DetectionBox->SetCollisionProfileName(TEXT("Trigger"));
    DetectionBox->SetGenerateOverlapEvents(true);
}

void AFallingPad::BeginPlay()
{
    Super::BeginPlay();

    if (DetectionBox)
    {
        DetectionBox->OnComponentBeginOverlap.AddDynamic(this, &AFallingPad::OnBoxOverlap);
    }
}

void AFallingPad::OnBoxOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (OtherActor && (OtherActor != this))
    {
        if (ACharacter* Player = Cast<ACharacter>(OtherActor))
        {
            DetectionBox->SetGenerateOverlapEvents(false);

            if (FallDelay > 0.0f)
            {
                GetWorldTimerManager().SetTimer(FallTimerHandle, this, &AFallingPad::StartFalling, FallDelay, false);
            }
            else
            {
                StartFalling();
            }
        }
    }
}

void AFallingPad::StartFalling()
{
    if (MeshComp)
    {
        MeshComp->SetSimulatePhysics(true);
    }
}