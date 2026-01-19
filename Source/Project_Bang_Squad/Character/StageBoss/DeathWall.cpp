#include "DeathWall.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Project_Bang_Squad/Core/TrueDamageType.h" // 아까 만든 트루 데미지

ADeathWall::ADeathWall()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true; // 서버에서 움직이면 클라도 따라 움직임
    SetReplicateMovement(true);

    // 1. 루트 컴포넌트 (벽 메쉬)
    WallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallMesh"));
    RootComponent = WallMesh;
    WallMesh->SetCollisionProfileName(TEXT("BlockAll")); // 발판 역할도 해야 하므로 Block

    // 2. 킬존 (벽의 앞면에 배치할 트리거)
    KillZone = CreateDefaultSubobject<UBoxComponent>(TEXT("KillZone"));
    KillZone->SetupAttachment(WallMesh);
    KillZone->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

    // 박스 크기는 나중에 BP에서 벽 크기에 맞춰 조정
    KillZone->SetBoxExtent(FVector(50.f, 1000.f, 500.f));
    KillZone->SetRelativeLocation(FVector(60.f, 0.f, 250.f)); // 벽보다 살짝 튀어나오게
}

void ADeathWall::BeginPlay()
{
    Super::BeginPlay();

    // 서버에서만 충돌 검사 수행
    if (HasAuthority())
    {
        KillZone->OnComponentBeginOverlap.AddDynamic(this, &ADeathWall::OnOverlapBegin);
    }
}

void ADeathWall::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 서버에서만 이동 로직 수행
    if (HasAuthority() && bIsActive)
    {
        FVector NewLocation = GetActorLocation() + (GetActorForwardVector() * MoveSpeed * DeltaTime);
        SetActorLocation(NewLocation);
    }
}

void ADeathWall::ActivateWall()
{
    if (HasAuthority())
    {
        bIsActive = true;
    }
}

void ADeathWall::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!HasAuthority()) return;

    // 플레이어인지 확인
    if (OtherActor && OtherActor != this && OtherActor->ActorHasTag("Player")) // 태그 혹은 캐스팅으로 확인
    {
        // 트루 데미지로 즉사 처리
        UGameplayStatics::ApplyDamage(
            OtherActor,
            KillDamage,
            nullptr, // 가해자 컨트롤러 (없음)
            this,    // 가해자 액터 (벽)
            UTrueDamageType::StaticClass()
        );
    }
}