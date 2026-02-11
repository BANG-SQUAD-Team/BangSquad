#include "Project_Bang_Squad/MapPuzzle/RisingPlatform.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Curves/CurveFloat.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraShakeBase.h"

ARisingPlatform::ARisingPlatform()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;
	MeshComp->SetMobility(EComponentMobility::Movable);
}

void ARisingPlatform::BeginPlay()
{
	Super::BeginPlay();

	StartLocation = GetActorLocation();
	EndLocation = StartLocation + FVector(0, 0, RiseHeight);

	if (RiseCurve)
	{
		float MinTime, MaxTime;
		RiseCurve->GetTimeRange(MinTime, MaxTime);
		MaxCurveTime = MaxTime;
	}
}

void ARisingPlatform::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsRising && RiseCurve)
	{
		CurrentCurveTime += DeltaTime;

		float Alpha = RiseCurve->GetFloatValue(CurrentCurveTime);
		FVector NewLoc = FMath::Lerp(StartLocation, EndLocation, Alpha);
		SetActorLocation(NewLoc);

		if (CurrentCurveTime >= MaxCurveTime)
		{
			bIsRising = false;
			SetActorLocation(EndLocation);

			if (ArrivalCameraShake)
			{
				APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
				if (PC)
				{
					PC->ClientStartCameraShake(ArrivalCameraShake, 1.0f);
				}
			}

			// 2. 사운드 재생 (나중에 주석 풀고 사용)
			/*
			if (ArrivalSound)
			{
				UGameplayStatics::PlaySoundAtLocation(this, ArrivalSound, GetActorLocation());
			}
			*/
		}
	}
}

void ARisingPlatform::Multicast_TriggerRise_Implementation()
{
	bIsRising = true;
	CurrentCurveTime = 0.0f;
}