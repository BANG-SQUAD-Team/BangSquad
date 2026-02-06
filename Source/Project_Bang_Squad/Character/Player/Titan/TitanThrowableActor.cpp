#include "Project_Bang_Squad/Character/Player/Titan/TitanThrowableActor.h"

ATitanThrowableActor::ATitanThrowableActor()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true); // 잡을 때는 TitanCharacter가 이걸 끄고, 던지면 다시 켭니다.

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;

	MeshComp->SetMobility(EComponentMobility::Movable);
	MeshComp->SetSimulatePhysics(true);
	MeshComp->SetEnableGravity(true);

	MeshComp->SetCollisionProfileName(TEXT("PhysicsActor"));
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

void ATitanThrowableActor::BeginPlay()
{
	Super::BeginPlay();

}

void ATitanThrowableActor::OnThrown_Implementation(FVector Direction)
{
}