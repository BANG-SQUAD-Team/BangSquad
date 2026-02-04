#include "ShatterPlatform.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Components/BoxComponent.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h" // 캐릭터 헤더 경로 확인 필요

AShatterPlatform::AShatterPlatform()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// 1. 돌멩이 (GC) 컴포넌트
	GCComponent = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("GCComponent"));
	SetRootComponent(GCComponent);

	// 초기 설정: 물리는 꺼두고, 충돌은 켜둠 (총알 막기용 등)
	GCComponent->SetSimulatePhysics(false);
	GCComponent->SetCollisionProfileName(TEXT("BlockAll"));

	// 2. [추가] 실제 밟는 바닥 (투명 큐브)
	MainFloorCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("MainFloorCollision"));
	MainFloorCollision->SetupAttachment(GCComponent);
	// 크기는 에디터에서 발판 크기에 맞춰 조절하세요 (기본값)
	MainFloorCollision->SetBoxExtent(FVector(150.f, 150.f, 20.f));
	MainFloorCollision->SetCollisionProfileName(TEXT("BlockAll")); // 확실하게 밟힘
	MainFloorCollision->SetHiddenInGame(true); // 게임에선 안 보임 (투명)

	// 3. 감지용 트리거 박스
	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(GCComponent);
	TriggerBox->SetBoxExtent(FVector(200.f, 200.f, 100.f)); // 바닥보다 좀 더 높게 설정
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
	if (!HasAuthority() || !TriggerBox) return;

	TArray<AActor*> OverlappingActors;
	TriggerBox->GetOverlappingActors(OverlappingActors, ABaseCharacter::StaticClass());

	int32 CurrentCount = OverlappingActors.Num();

	// 로그 출력
	UE_LOG(LogTemp, Warning, TEXT("[ShatterPlatform] Players: %d / %d"), CurrentCount, RequiredPlayerCount);

	if (CurrentCount >= RequiredPlayerCount)
	{
		bIsShattered = true;
		Multicast_Shatter();
	}
}

void AShatterPlatform::Multicast_Shatter_Implementation()
{
	if (MainFloorCollision)
	{
		MainFloorCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (!GCComponent) return;

	GCComponent->SetSimulatePhysics(true);
	GCComponent->WakeAllRigidBodies();

	GCComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GCComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	FVector Loc = GetActorLocation();
	GCComponent->ApplyExternalStrain(ShatterDamage, Loc, 50000.0f);

	GCComponent->AddRadialImpulse(Loc, 3000.0f, 5000.0f, ERadialImpulseFalloff::RIF_Linear, true);

	if (TriggerBox)
	{
		TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	SetLifeSpan(CleanupDelay);
}