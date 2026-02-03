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

	// 초기 설정
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

	UE_LOG(LogTemp, Warning, TEXT("[ShatterPlatform] BeginPlay Started."));

	if (HasAuthority())
	{
		if (TriggerBox)
		{
			TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AShatterPlatform::OnOverlapBegin);
			TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AShatterPlatform::OnOverlapEnd);
			UE_LOG(LogTemp, Warning, TEXT("[ShatterPlatform] Server: TriggerBox events bound."));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[ShatterPlatform] Server: TriggerBox is NULL!"));
		}
	}
}

void AShatterPlatform::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bIsShattered) return;

	// 겹친 물체가 뭔지 로그 찍기
	FString ActorName = OtherActor ? OtherActor->GetName() : TEXT("NULL");
	UE_LOG(LogTemp, Log, TEXT("[ShatterPlatform] Overlap Begin: %s"), *ActorName);

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

	int32 CurrentCount = OverlappingActors.Num();

	// [중요] 현재 몇 명인지 로그 출력 (화면 + 출력 로그)
	FString Msg = FString::Printf(TEXT("[ShatterPlatform] Current Players: %d / Required: %d"), CurrentCount, RequiredPlayerCount);
	UE_LOG(LogTemp, Warning, TEXT("%s"), *Msg);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, Msg);
	}

	if (CurrentCount >= RequiredPlayerCount)
	{
		UE_LOG(LogTemp, Error, TEXT("[ShatterPlatform] Requirement Met! Calling Multicast_Shatter..."));
		bIsShattered = true;
		Multicast_Shatter();
	}
}

void AShatterPlatform::Multicast_Shatter_Implementation()
{
	UE_LOG(LogTemp, Error, TEXT("[ShatterPlatform] Multicast_Shatter Executing!"));

	if (!GCComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("[ShatterPlatform] GCComponent is NULL!"));
		return;
	}

	// 1. 물리 시뮬레이션 켜기
	GCComponent->SetSimulatePhysics(true);
	GCComponent->WakeAllRigidBodies(); // 자는 물리 깨우기
	UE_LOG(LogTemp, Warning, TEXT("[ShatterPlatform] Physics Enabled & Woken up."));

	// 2. 외부 충격 가하기
	// 데미지 수치와 위치 로그
	FVector Loc = GetActorLocation();
	UE_LOG(LogTemp, Warning, TEXT("[ShatterPlatform] Applying Strain: %f at Location: %s"), ShatterDamage, *Loc.ToString());

	GCComponent->ApplyExternalStrain(ShatterDamage, Loc, 500.0f);

	// 3. 트리거 비활성화
	if (TriggerBox)
	{
		TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 4. 삭제 타이머
	UE_LOG(LogTemp, Warning, TEXT("[ShatterPlatform] Destroy Timer Started (%f sec)"), CleanupDelay);
	SetLifeSpan(CleanupDelay);
}