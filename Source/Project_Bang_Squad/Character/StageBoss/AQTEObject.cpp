// [AQTEObject.cpp]
#include "AQTEObject.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

AQTEObject::AQTEObject()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    RootComponent = MeshComp;
}

void AQTEObject::BeginPlay()
{
    Super::BeginPlay();
}

void AQTEObject::InitializeFalling(AActor* Target, float Duration)
{
    if (!HasAuthority()) return;

    FallDuration = Duration;
    ElapsedTime = 0.0f;
    StartLocation = GetActorLocation();

    if (Target)
    {
        TargetLocation = Target->GetActorLocation();
        TargetLocation.Z -= 100.0f; // ¹Ù´Ú º¸Á¤
    }
    else
    {
        TargetLocation = StartLocation - FVector(0, 0, 2000.0f);
    }

    bIsFalling = true;
}

void AQTEObject::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (HasAuthority() && bIsFalling)
    {
        ElapsedTime += DeltaTime;
        float Alpha = FMath::Clamp(ElapsedTime / FallDuration, 0.0f, 1.0f);
        SetActorLocation(FMath::Lerp(StartLocation, TargetLocation, Alpha));
    }
}

void AQTEObject::TriggerSuccess()
{
    if (HasAuthority())
    {
        Multicast_PlayExplosion(true);
        SetLifeSpan(1.0f);
    }
}

void AQTEObject::TriggerFailure()
{
    if (HasAuthority())
    {
        Multicast_PlayExplosion(false);
        SetLifeSpan(0.5f);
    }
}

void AQTEObject::Multicast_PlayExplosion_Implementation(bool bIsSuccess)
{
    if (ExplosionFX)
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionFX, GetActorLocation(), GetActorRotation(), FVector(3.0f));
    }
}