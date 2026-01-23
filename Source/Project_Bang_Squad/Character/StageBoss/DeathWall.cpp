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

    // 리듬 패턴 관련 변수 초기화 (GeneratePlatforms 함수 내에서 설정됩니다)
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
        // [수정] Forward(앞) 대신 Right(오른쪽) 벡터 사용
        // 만약 반대로 가면 -GetActorRightVector() 로 바꾸시면 됩니다.
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

    // ★ 랜덤 방향 결정 (True면 왼쪽이 돌, False면 오른쪽이 돌)
    bool bStoneOnLeft = FMath::RandBool();

    float StoneMainDir = bStoneOnLeft ? -1.0f : 1.0f;
    float PadMainDir = bStoneOnLeft ? 1.0f : -1.0f;


    // ----------------------------------------------------
    // [1] 공통 구간 (2개 생성 - 확실한 계단 만들기)
    // ----------------------------------------------------
    float CurrentZ = BoxBottomZ + 50.0f;

    // 1번 발판 (바닥) - 완전 중앙
    FVector Pos1 = BoxOrigin;
    Pos1.Z = CurrentZ;
    Pos1 += FwdVec * StickOut;
    Pos1 += RightVec * 0.0f;
    GetWorld()->SpawnActor<AActor>(PlatformClass, Pos1, GetActorRotation())
        ->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);

    // 2번 발판 (분기점) - 옆으로 이동
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
    // [2] 돌 발판 길 (Stone Path) - 기존 유지
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

        // 반대편(점프패드 구역) 침범 방지
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
    // [3] 점프 패드 길 (Jump Pad Path) - ★ 수정됨
    // ====================================================
    if (JumpPadClass)
    {
        float PadZ = ForkZ + 120.0f;
        float PadY = ForkY + (PadMainDir * 300.0f);

        // 첫 점프 패드
        FVector FirstPadPos = BoxOrigin;
        FirstPadPos.Z = PadZ;
        FirstPadPos += FwdVec * StickOut;
        FirstPadPos += RightVec * PadY;

        AActor* FirstPad = GetWorld()->SpawnActor<AActor>(JumpPadClass, FirstPadPos, GetActorRotation());
        if (FirstPad) FirstPad->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);

        PadZ += GridHeight;

        // ★ [수정] 간격을 3~4칸으로 확 줄임 (촘촘하게 생성)
        int32 PadSkip = FMath::RandRange(3, 4);
        float ZigZag = 1.0f;

        while (PadZ < BoxTopZ)
        {
            PadSkip--;
            if (PadSkip <= 0)
            {
                ZigZag *= -1.0f;
                PadY += (ZigZag * 100.0f) + FMath::RandRange(-30.0f, 30.0f);

                // 반대편(돌 구역) 침범 방지
                if (bStoneOnLeft) // 패드는 오른쪽
                {
                    if (PadY < 50.0f) PadY = 100.0f;
                    if (PadY > MaxY) PadY = MaxY - 50.0f;
                }
                else // 패드는 왼쪽
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

                // ★ [수정] 다음 패드도 3~4칸 뒤에 바로 생성
                PadSkip = FMath::RandRange(5, 7);
            }
            PadZ += GridHeight;
        }
    }
}

void ADeathWall::SpawnRow(float Z, int32 CenterIdx, int32 Size, float Width, FVector Origin, FVector Fwd, FVector Right)
{
    for (int32 i = 0; i < Size; i++)
    {
        int32 Offset = (i % 2 == 0) ? (i / 2) : -((i + 1) / 2);
        int32 FinalIdx = CenterIdx + Offset;

        float Y_Pos = FinalIdx * Width;
        Y_Pos += FMath::RandRange(-20.0f, 20.0f);

        FVector SpawnPos = Origin;
        SpawnPos.Z = Z;
        SpawnPos += Fwd * PlatformStickOut;
        SpawnPos += Right * Y_Pos;

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;

        AActor* NewPlatform = GetWorld()->SpawnActor<AActor>(PlatformClass, SpawnPos, GetActorRotation(), SpawnParams);
        if (NewPlatform)
        {
            NewPlatform->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
        }
    }
}

void ADeathWall::SpawnCluster(float Z, int32 CenterIdx, int32 Size, float Width, FVector Origin, FVector Fwd, FVector Right)
{
    // 뭉치를 만들 때 CenterIdx를 중심으로 좌우로 퍼지게 할지, 한쪽으로 붙일지 결정
    int32 StartIdx = CenterIdx;

    for (int32 i = 0; i < Size; i++)
    {
        int32 Offset = (i % 2 == 0) ? (i / 2) : -((i + 1) / 2);
        int32 TargetIdx = CenterIdx + Offset;

        float Y_Pos = TargetIdx * Width;
        Y_Pos += FMath::RandRange(-15.0f, 15.0f);

        FVector SpawnPos = Origin;
        SpawnPos.Z = Z;
        SpawnPos += Fwd * PlatformStickOut;
        SpawnPos += Right * Y_Pos;

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;

        AActor* NewPlatform = GetWorld()->SpawnActor<AActor>(PlatformClass, SpawnPos, GetActorRotation(), SpawnParams);
        if (NewPlatform)
        {
            NewPlatform->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
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