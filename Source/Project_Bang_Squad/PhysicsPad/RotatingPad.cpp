#include "RotatingPad.h"
#include "Components/StaticMeshComponent.h"

ARotatingPad::ARotatingPad()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    AActor::SetReplicateMovement(true);

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    RootComponent = MeshComp;
}

void ARotatingPad::BeginPlay()
{
    Super::BeginPlay();
    Timer = 0.0f;
    CurrentState = ERotateState::Rotating;
}

void ARotatingPad::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (HasAuthority())
    {
        Timer += DeltaTime;

        if (CurrentState == ERotateState::Rotating)
        {
            float Alpha = FMath::Clamp(Timer / RotateDuration, 0.0f, 1.0f);
            float TargetAlpha = bTargetingMax ? Alpha : 1.0f - Alpha;
            UpdateRotation(TargetAlpha);

            if (Timer >= RotateDuration)
            {
                Timer = 0.0f;
                CurrentState = ERotateState::Waiting;
            }
        }
        else if (CurrentState == ERotateState::Waiting)
        {
            if (Timer >= StayDuration)
            {
                Timer = 0.0f;
                bTargetingMax = !bTargetingMax;
                CurrentState = ERotateState::Rotating;
            }
        }
    }
}

void ARotatingPad::UpdateRotation(float Alpha)
{
    float CurrentRoll = FMath::Lerp(0.0f, 180.0f, Alpha);
    SetActorRelativeRotation(FRotator(0.0f, 0.0f, CurrentRoll));
}