
#include "Project_Bang_Squad/Game/MapPattern/MagicGate.h"


AMagicGate::AMagicGate()
{
	PrimaryActorTick.bCanEverTick = true;
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>("MeshComp");
	RootComponent = MeshComp;
	
	bReplicates = true;
	SetReplicateMovement(true);

}


void AMagicGate::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (bIsOpen)
	{
		// 현재 위치에서 목표 위치(TargetZ)로 부드럽게 이동 
		FVector CurrentLoc = GetActorLocation();
		float NewZ = FMath::FInterpTo(CurrentLoc.Z, TargetZ, DeltaTime, 2.0f);
		
		SetActorLocation(FVector(CurrentLoc.X, CurrentLoc.Y, NewZ));
	}
}

void AMagicGate::OpenGate()
{
	if (!bIsOpen)
	{
		bIsOpen = true;
		// 현재 위치에서 아래로 내려가도록 목표 설정
		TargetZ = GetActorLocation().Z - OpenDistance;
	}
}

