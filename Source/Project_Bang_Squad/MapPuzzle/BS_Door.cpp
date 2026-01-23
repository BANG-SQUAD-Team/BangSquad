#include "BS_Door.h"
#include "GameFramework/GameStateBase.h"
#include "Math/UnrealMathUtility.h"
#include "Net/UnrealNetwork.h"

ABS_Door::ABS_Door()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	LeftDoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftDoor"));
	LeftDoorMesh->SetupAttachment(RootComponent);

	RightDoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightDoor"));
	RightDoorMesh->SetupAttachment(RootComponent);
}

void ABS_Door::OpenDoor()
{
	if (!HasAuthority() || bIsOpen) return;
	bIsOpen = true;
	OpenStartTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
}

void ABS_Door::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bIsOpen && OpenStartTime > 0.f)
    {
        if (AGameStateBase* GS = GetWorld()->GetGameState())
        {
            float CurrentTime = (float)GS->GetServerWorldTimeSeconds();
            float Alpha = FMath::Clamp((CurrentTime - OpenStartTime) / OpenDuration, 0.f, 1.f);
            float SmoothAlpha = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);

            // 1. 왼쪽 문 (시작: 0도)
            // 0도에서 시작해서 설정한 Offset(110도)까지 회전
            float LeftNewYaw = FMath::Lerp<float>(0.f, LeftTargetAngle, SmoothAlpha);
            LeftDoorMesh->SetRelativeRotation(FRotator(0.f, LeftNewYaw, 0.f));

            // 2. 오른쪽 문 (시작: -180도)
            // -180도에서 시작해서 (-180 + Offset)까지 회전
            // 만약 Offset이 -110이라면 최종적으로 -290도까지 돌아갑니다.
            float RightStartYaw = -180.f;
            float RightNewYaw = FMath::Lerp<float>(RightStartYaw, RightStartYaw + RightTargetAngle, SmoothAlpha);
            RightDoorMesh->SetRelativeRotation(FRotator(0.f, RightNewYaw, 0.f));
        }
    }
}

void ABS_Door::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABS_Door, bIsOpen);
	DOREPLIFETIME(ABS_Door, OpenStartTime);
}