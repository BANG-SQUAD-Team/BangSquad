#include "ShatterPlatform.h"

#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Components/BoxComponent.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h" 

AShatterPlatform::AShatterPlatform()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	GCComponent = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("GCComponent"));
	SetRootComponent(GCComponent);

	// 물리 시뮬레이션 끄기 (고정 상태)
	GCComponent->SetSimulatePhysics(false);
	GCComponent->SetCollisionProfileName(TEXT("BlockAll"));

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(GCComponent);
	TriggerBox->SetBoxExtent(FVector(200.f, 200.f, 100.f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
}

void AShatterPlatform::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		if (TriggerBox)
		{
			TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AShatterPlatform::OnOverlapBegin);
			TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AShatterPlatform::OnOverlapEnd);
		}
	}
}

void AShatterPlatform::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bIsShattered) return;
	CheckPlayerCount();
}

void AShatterPlatform::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (bIsShattered) return;
	CheckPlayerCount();
}

void AShatterPlatform::CheckPlayerCount()
{
	if (!HasAuthority()) return;
	if (!TriggerBox) return;

	TArray<AActor*> OverlappingActors;
	TriggerBox->GetOverlappingActors(OverlappingActors, ABaseCharacter::StaticClass());

	if (GEngine)
	{
		FString Msg = FString::Printf(TEXT("Current Players: %d / %d"), OverlappingActors.Num(), RequiredPlayerCount);
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, Msg);
	}

	if (OverlappingActors.Num() >= RequiredPlayerCount)
	{
		bIsShattered = true;
		Multicast_Shatter();
	}
}

void AShatterPlatform::Multicast_Shatter_Implementation()
{
	if (!GCComponent) return;

	// [수정됨] SetObjectType 삭제. 물리 시뮬레이션 활성화만으로 충분합니다.
	GCComponent->SetSimulatePhysics(true);

	// 2. 외부 충격 가하기 (와장창)
	GCComponent->ApplyExternalStrain(ShatterDamage, GetActorLocation(), 500.0f);

	// 3. 트리거 비활성화
	if (TriggerBox)
	{
		TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 4. 삭제 타이머
	SetLifeSpan(CleanupDelay);
}