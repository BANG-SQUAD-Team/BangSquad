#include "DeathWall.h"
#include "Kismet/KismetMathLibrary.h" 
#include "Kismet/GameplayStatics.h"
#include "Project_Bang_Squad/BossPattern/Boss1_Rampart.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"

ADeathWall::ADeathWall()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);

    // 1. 벽 메쉬
    WallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallMesh"));
    RootComponent = WallMesh;
    WallMesh->SetCollisionProfileName(TEXT("BlockAll"));

    // 2. 생성 범위 박스
    SpawnVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnVolume"));
    SpawnVolume->SetupAttachment(RootComponent);
    SpawnVolume->SetLineThickness(5.0f);
    SpawnVolume->ShapeColor = FColor::Red;
    SpawnVolume->SetCollisionProfileName(TEXT("NoCollision"));

    // 박스 기본 크기
    SpawnVolume->SetBoxExtent(FVector(100.0f, 1000.0f, 1000.0f));

    // [설정값 초기화]
    FirstPlatformHeight = 60.0f;
    LayerHeight = 150.0f;
    BranchWidth = 120.0f;
    PlatformStickOut = 150.0f;
    BranchProbability = 0.6f;
}

void ADeathWall::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        GeneratePlatforms();

        // 성벽 충돌 무시 로직
        TArray<AActor*> FoundRamparts;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABoss1_Rampart::StaticClass(), FoundRamparts);

        for (AActor* Actor : FoundRamparts)
        {
            if (ABoss1_Rampart* Rampart = Cast<ABoss1_Rampart>(Actor))
            {
                if (WallMesh) WallMesh->IgnoreActorWhenMoving(Rampart, true);
                if (UPrimitiveComponent* RampartRoot = Cast<UPrimitiveComponent>(Rampart->GetRootComponent()))
                {
                    RampartRoot->IgnoreActorWhenMoving(this, true);
                }
            }
        }
    }
}

void ADeathWall::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (HasAuthority() && bIsActive)
    {
        FVector MoveDir = GetActorRightVector();
        AddActorWorldOffset(MoveDir * MoveSpeed * DeltaTime);
    }
}

void ADeathWall::GeneratePlatforms()
{
    if (!PlatformClass) return;

    FVector BoxExtent = SpawnVolume->GetScaledBoxExtent();
    FVector BoxOrigin = SpawnVolume->GetComponentLocation();
    FVector FwdVec = SpawnVolume->GetForwardVector();
    FVector RightVec = SpawnVolume->GetRightVector();

    float BoxBottomZ = BoxOrigin.Z - BoxExtent.Z;
    float BoxTopZ = BoxOrigin.Z + BoxExtent.Z;

    // ====================================================
    // [설정값]
    // ====================================================
    float GridHeight = 110.0f;
    float StickOut = 200.0f;
    float MaxY = BoxExtent.Y - 100.0f;

    // 방향 결정
    bool bStoneOnLeft = FMath::RandBool();

    float StoneMainDir = bStoneOnLeft ? -1.0f : 1.0f;
    float PadMainDir = bStoneOnLeft ? 1.0f : -1.0f;


    // ----------------------------------------------------
    // [1] 공통 구간
    // ----------------------------------------------------
    float CurrentZ = BoxBottomZ + 50.0f;

    // 1번 발판
    FVector Pos1 = BoxOrigin;
    Pos1.Z = CurrentZ;
    Pos1 += FwdVec * StickOut;
    Pos1 += RightVec * 0.0f;
    GetWorld()->SpawnActor<AActor>(PlatformClass, Pos1, GetActorRotation())
        ->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);

    // 2번 발판
    CurrentZ += GridHeight;
    float ForkY = StoneMainDir * 200.0f;

    FVector Pos2 = BoxOrigin;
    Pos2.Z = CurrentZ;
    Pos2 += FwdVec * StickOut;
    Pos2 += RightVec * ForkY;
    GetWorld()->SpawnActor<AActor>(PlatformClass, Pos2, GetActorRotation())
        ->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);

    float ForkZ = CurrentZ;


    // ====================================================
    // [2] 돌 발판 길
    // ====================================================
    float StoneZ = ForkZ + GridHeight;
    float StoneY = ForkY + (StoneMainDir * 200.0f);

    float StoneWanderDir = StoneMainDir;
    int32 StoneSteps = 0;
    int32 StoneTarget = 4;

    while (StoneZ < BoxTopZ)
    {
        if (FMath::Abs(StoneY + (StoneWanderDir * 300.0f)) > MaxY)
            StoneWanderDir *= -1.0f;

        if (StoneSteps >= StoneTarget) {
            StoneWanderDir *= -1.0f;
            StoneSteps = 0;
            StoneTarget = FMath::RandRange(3, 5);
        }

        StoneY += (StoneWanderDir * FMath::RandRange(200.0f, 350.0f));

        // 침범 방지
        if (bStoneOnLeft)
        {
            if (StoneY > -50.0f) StoneY = -50.0f;
            if (StoneY < -MaxY) StoneY = -MaxY + 50.0f;
        }
        else
        {
            if (StoneY < 50.0f) StoneY = 50.0f;
            if (StoneY > MaxY) StoneY = MaxY - 50.0f;
        }

        FVector SpawnPos = BoxOrigin;
        SpawnPos.Z = StoneZ;
        SpawnPos += FwdVec * StickOut;
        SpawnPos += RightVec * StoneY;

        AActor* NewPlat = GetWorld()->SpawnActor<AActor>(PlatformClass, SpawnPos, GetActorRotation());
        if (NewPlat) NewPlat->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);

        StoneZ += GridHeight;
        StoneSteps++;
    }


    // ====================================================
    // [3] 점프 패드 길
    // ====================================================
    if (JumpPadClass)
    {
        float PadZ = ForkZ + 120.0f;
        float PadY = ForkY + (PadMainDir * 300.0f);

        // 첫 패드
        FVector FirstPadPos = BoxOrigin;
        FirstPadPos.Z = PadZ;
        FirstPadPos += FwdVec * StickOut;
        FirstPadPos += RightVec * PadY;

        AActor* FirstPad = GetWorld()->SpawnActor<AActor>(JumpPadClass, FirstPadPos, GetActorRotation());
        if (FirstPad) FirstPad->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);

        PadZ += GridHeight;

        int32 PadSkip = FMath::RandRange(3, 4);
        float ZigZag = 1.0f;

        while (PadZ < BoxTopZ)
        {
            PadSkip--;
            if (PadSkip <= 0)
            {
                ZigZag *= -1.0f;
                PadY += (ZigZag * 100.0f) + FMath::RandRange(-30.0f, 30.0f);

                if (bStoneOnLeft)
                {
                    if (PadY < 50.0f) PadY = 100.0f;
                    if (PadY > MaxY) PadY = MaxY - 50.0f;
                }
                else
                {
                    if (PadY > -50.0f) PadY = -100.0f;
                    if (PadY < -MaxY) PadY = -MaxY + 50.0f;
                }

                FVector PadPos = BoxOrigin;
                PadPos.Z = PadZ;
                PadPos += FwdVec * StickOut;
                PadPos += RightVec * PadY;

                AActor* NewPad = GetWorld()->SpawnActor<AActor>(JumpPadClass, PadPos, GetActorRotation());
                if (NewPad) NewPad->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);

                PadSkip = FMath::RandRange(5, 7);
            }
            PadZ += GridHeight;
        }
    }
}

void ADeathWall::ActivateWall()
{
    if (HasAuthority()) bIsActive = true;
}

void ADeathWall::DeactivateWall()
{
    if (HasAuthority()) bIsActive = false;
}