#include "BS_Door.h"
#include "Net/UnrealNetwork.h"

ABS_Door::ABS_Door() {
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	LeftDoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftDoor"));
	LeftDoorMesh->SetupAttachment(RootComponent);
	RightDoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightDoor"));
	RightDoorMesh->SetupAttachment(RootComponent);
}

void ABS_Door::ExecuteAction(EDoorAction Action) {
	if (!HasAuthority()) return;
	switch (Action) {
	case EDoorAction::Open: bMasterOpen = true; break;
	case EDoorAction::Close: bMasterOpen = false; bTempOpen = false; break;
	case EDoorAction::TempOpen: bTempOpen = true; break;
	}
}

void ABS_Door::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);
	// 마스터가 열렸거나 일시적으로 열렸을 때 작동
	float TargetAlpha = (bMasterOpen || bTempOpen) ? 1.f : 0.f;
	CurrentAlpha = FMath::FInterpTo(CurrentAlpha, TargetAlpha, DeltaTime, 1.f / OpenDuration * 3.f);

	float LeftYaw = FMath::Lerp(0.f, LeftTargetAngle, CurrentAlpha);
	LeftDoorMesh->SetRelativeRotation(FRotator(0.f, LeftYaw, 0.f));

	float RightNewYaw = FMath::Lerp(-180.f, -180.f + RightTargetAngle, CurrentAlpha);
	RightDoorMesh->SetRelativeRotation(FRotator(0.f, RightNewYaw, 0.f));
}

void ABS_Door::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABS_Door, bMasterOpen);
	DOREPLIFETIME(ABS_Door, bTempOpen);
}