#include "BS_Switch.h"
#include "BS_Door.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"

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
	if (!HasAuthority() || bIsActivated) return; // 서버에서만 실행 및 중복 방지

	bIsActivated = true;
	ActivationTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();

	if (TargetDoor)
	{
		TargetDoor->OpenDoor(); // 문에게 열리라고 명령
	}
}

void ABS_Switch::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsActivated && ActivationTime > 0.f)
	{
		float CurrentTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
		float Alpha = FMath::Clamp((CurrentTime - ActivationTime) / 1.0f, 0.f, 1.f); // 1초 동안 이동

		// Y축으로 +14 이동
		FVector TargetLocation = InitialHandleLocation + FVector(0.f, 14.f, 0.f);
		HandleMesh->SetRelativeLocation(FMath::Lerp(InitialHandleLocation, TargetLocation, Alpha));
	}
}

void ABS_Switch::OnRep_IsActivated() { /* 필요한 경우 사운드나 이펙트 추가 */ }

void ABS_Switch::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABS_Switch, bIsActivated);
	DOREPLIFETIME(ABS_Switch, ActivationTime);
}