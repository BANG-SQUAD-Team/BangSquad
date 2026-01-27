#include "Project_Bang_Squad/Game/MapPattern/Cauldron.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Project_Bang_Squad/Game/MapPattern/MagicGate.h"

ACauldron::ACauldron()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;
    
	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(RootComponent);
    
	TriggerBox->SetBoxExtent(FVector(60.f, 60.f, 60.f));
    
	// 1. 프로필 설정
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	
	// 코드로 강제로 "PhysicsBody(버섯)와 겹침 허용"을 켭니다.
	TriggerBox->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
}


void ACauldron::BeginPlay()
{
	Super::BeginPlay();
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ACauldron::OnOverlapBegin);
}

void ACauldron::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) return;

	//  [디버그 로그] 뭐가 닿았는지 출력해봅니다.
	if (OtherActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("솥에 닿은 물체: %s"), *OtherActor->GetName());
	}

	if (!OtherActor || !TargetGate) return;
    
	if (OtherActor->ActorHasTag("Mushroom"))
	{
		UE_LOG(LogTemp, Warning, TEXT("버섯 감지됨! 문을 엽니다.")); // 
		TargetGate->OpenGate();
		OtherActor->Destroy();
	}
}