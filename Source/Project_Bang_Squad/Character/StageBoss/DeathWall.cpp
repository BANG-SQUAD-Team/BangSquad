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

    // [1] 루트 컴포넌트 생성 (기준점)
    DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
    RootComponent = DefaultSceneRoot;

    // [2] 벽 메쉬 생성 및 루트에 부착
    WallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallMesh"));
    WallMesh->SetupAttachment(DefaultSceneRoot); // 루트 밑에 자식으로 붙임
    WallMesh->SetCollisionProfileName(TEXT("BlockAll"));

    // ★ 여기서 회전! (이제 루트는 가만히 있고 메쉬만 돌아갑니다)
    // -90도로 해서 정면을 맞춥니다. 만약 반대면 90.0f로 바꾸세요.
    WallMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

    // [3] 생성 범위 박스 (루트에 부착해서 이동 방향과 일치시킴)
    SpawnVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnVolume"));
    SpawnVolume->SetupAttachment(DefaultSceneRoot);
    SpawnVolume->SetLineThickness(5.0f);
    SpawnVolume->ShapeColor = FColor::Red;
    SpawnVolume->SetCollisionProfileName(TEXT("NoCollision"));
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
        FVector MoveDir = GetActorForwardVector();
        AddActorWorldOffset(MoveDir * MoveSpeed * DeltaTime);
    }
}

void ADeathWall::GeneratePlatforms()
{
    if (!PlatformClass || !JumpPadClass) return;

    // ====================================================
    // 1. 기본 설정 (80% vs 20%)
    // ====================================================
    FVector BoxExtent = SpawnVolume->GetScaledBoxExtent();
    FVector BoxOrigin = SpawnVolume->GetComponentLocation();
    FVector FwdVec = SpawnVolume->GetForwardVector();
    FVector RightVec = SpawnVolume->GetRightVector();

    float BoxBottomZ = BoxOrigin.Z - BoxExtent.Z;
    float BoxTopZ = BoxOrigin.Z + BoxExtent.Z;
    float TotalWidth = BoxExtent.Y * 2.0f;
    float PadZoneWidth = TotalWidth * 0.2f;

    // 회전값 (90도)
    FRotator BaseRot = GetActorRotation();
    FRotator PlatformRot = BaseRot;
    PlatformRot.Yaw += 90.0f;

    bool bStoneOnLeft = FMath::RandBool();

    float LeftWall = -BoxExtent.Y + 50.0f;
    float RightWall = BoxExtent.Y - 50.0f;

    float DividerY = 0.0f;
    float StoneMinY, StoneMaxY, PadMinY, PadMaxY;
    float StoneDir = 0.0f; // -1: 왼쪽, 1: 오른쪽
    float PadDir = 0.0f;   // 돌과 반대

    if (bStoneOnLeft)
    {
        StoneDir = -1.0f;
        PadDir = 1.0f; // 패드는 오른쪽
        DividerY = RightWall - PadZoneWidth;
        StoneMinY = LeftWall;
        StoneMaxY = DividerY - 50.0f;
        PadMinY = DividerY + 50.0f;
        PadMaxY = RightWall;
    }
    else
    {
        StoneDir = 1.0f;
        PadDir = -1.0f; // 패드는 왼쪽
        DividerY = LeftWall + PadZoneWidth;
        PadMinY = LeftWall;
        PadMaxY = DividerY - 50.0f;
        StoneMinY = DividerY + 50.0f;
        StoneMaxY = RightWall;
    }

    // 설정값
    float MinStairGap = 220.0f;
    float MaxStairGap = 450.0f;
    float MinLongFlat = 560.0f;
    float MaxLongFlat = 760.0f;
    float BaseShortUp = 150.0f;
    float TurnJumpDist = 660.0f;
    int32 MinTailCount = 4;
    float EssentialSpace = (BaseShortUp * MinTailCount) + 50.0f;

    float GridHeight = 110.0f;
    float StickOut = 200.0f;
    float MaxY = BoxExtent.Y - 100.0f;

    // 방향 결정
    bool bStoneOnLeft = FMath::RandBool();

    float StoneMainDir = bStoneOnLeft ? -1.0f : 1.0f;
    float PadMainDir = bStoneOnLeft ? 1.0f : -1.0f;

    // ----------------------------------------------------
    // [2] 공통 시작 구간 (Start)
    // ----------------------------------------------------
    // ★ [수정] 50도 높다고 하셔서 20으로 바닥에 붙임
    float CurrentZ = BoxBottomZ + 20.0f;

    // 1번 발판: 완전 중앙 바닥
    FVector Pos1 = BoxOrigin;
    Pos1.Z = CurrentZ;
    Pos1 += FwdVec * StickOut;
    GetWorld()->SpawnActor<AActor>(PlatformClass, Pos1, PlatformRot)->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);

    // 2번 발판: 돌 구역 시작점 (중앙에서 200만큼 옆으로)
    float StoneStartZ = CurrentZ + GridHeight;
    float StartY = StoneDir * 200.0f;

    FVector Pos2 = BoxOrigin;
    Pos2.Z = StoneStartZ;
    Pos2 += FwdVec * StickOut;
    Pos2 += RightVec * StartY;
    GetWorld()->SpawnActor<AActor>(PlatformClass, Pos2, PlatformRot)->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);

    float ForkZ = StoneStartZ;

    // ====================================================
    // [3] 돌 발판 루프 (80% 구역)
    // ====================================================
    float StoneZ = ForkZ + GridHeight;
    float StoneY = StartY;
    float MinTurnZ = ForkZ + (GridHeight * 4.0f);

    float CurrentDirection = StoneDir;

    int32 StepsInCurrentDir = 0;
    int32 HeadLength = FMath::RandRange(1, 2);
    int32 TailLength = MinTailCount;
    int32 TargetSteps = HeadLength + 1 + TailLength;

    while (StoneZ < BoxTopZ)
    {
        bool bIsJumpTurn = false;
        float CheckNextY = StoneY + (CurrentDirection * 450.0f);

        if (CheckNextY < StoneMinY || CheckNextY > StoneMaxY) bIsJumpTurn = true;
        else if (StoneZ > MinTurnZ && StepsInCurrentDir >= TargetSteps) bIsJumpTurn = true;

        float GapToUse = 0.0f;
        bool bIsFlatJump = false;

        if (bIsJumpTurn)
        {
            CurrentDirection *= -1.0f;
            float IdealY = StoneY + (CurrentDirection * TurnJumpDist);

            float LimitForTail = 0.0f;
            if (CurrentDirection > 0) LimitForTail = StoneMaxY - EssentialSpace;
            else LimitForTail = StoneMinY + EssentialSpace;

            if (CurrentDirection > 0) StoneY = (IdealY > LimitForTail) ? LimitForTail : IdealY;
            else StoneY = (IdealY < LimitForTail) ? LimitForTail : IdealY;

            StoneY = FMath::Clamp(StoneY, StoneMinY, StoneMaxY);

            float DistToLimit = (CurrentDirection > 0) ? (StoneMaxY - StoneY) : (StoneY - StoneMinY);
            float RemainingSpace = DistToLimit - (BaseShortUp * MinTailCount);
            int32 ExtraTails = (RemainingSpace > 0) ? FMath::FloorToInt(RemainingSpace / BaseShortUp) : 0;

            TailLength = MinTailCount + ExtraTails;
            if (TailLength > 8) TailLength = 8;

            HeadLength = FMath::RandRange(1, 3);
            StepsInCurrentDir = 0;
            TargetSteps = HeadLength + 1 + TailLength;
        }
        else
        {
            int32 RemSteps = TargetSteps - StepsInCurrentDir;

            if (RemSteps <= TailLength) GapToUse = BaseShortUp;
            else if (RemSteps == TailLength + 1) { GapToUse = FMath::RandRange(MinLongFlat, MaxLongFlat); bIsFlatJump = true; }
            else
            {
                GapToUse = FMath::RandRange(MinStairGap, MaxStairGap);
                if (StepsInCurrentDir > 0 && FMath::RandRange(0.0f, 1.0f) < 0.3f)
                {
                    float PredictY = StoneY + (CurrentDirection * (GapToUse + EssentialSpace));
                    if (PredictY >= StoneMinY && PredictY <= StoneMaxY) bIsFlatJump = true;
                }
            }
            StoneY += (CurrentDirection * GapToUse);
            if (bIsFlatJump) StoneZ -= GridHeight;
        }

        FVector SpawnPos = BoxOrigin;
        SpawnPos.Z = StoneZ;
        SpawnPos += FwdVec * StickOut;
        SpawnPos += RightVec * StoneY;

        AActor* NewPlat = GetWorld()->SpawnActor<AActor>(PlatformClass, SpawnPos, PlatformRot);
        if (NewPlat) NewPlat->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);

        StoneZ += GridHeight;
        StepsInCurrentDir++;
    }

    // ====================================================
    // [4] 점프 패드 루프 (20% 구역)
    // ====================================================
    // ★ [중요] 점프 패드 진입로 만들기
    // 문제는 '중앙'에서 '구석 패드존'까지 거리가 먼데 발판이 없어서 못 가는 것.
    // 중앙(0)에서 패드존(PadZoneCenter)까지 이어주는 "가로 징검다리"를 놓습니다.

    float PadZ = CurrentZ + 20.0f; // 1번 발판(중앙)보다 조금 위에서 시작
    float PadZoneCenter = (PadMinY + PadMaxY) * 0.5f;

    // 중앙(0)부터 패드 구역(PadZoneCenter)까지 3번에 나눠서 이동
    int32 BridgeSteps = 4;

    for (int32 i = 1; i <= BridgeSteps; ++i)
    {
        // 0.33, 0.66, 1.0 비율로 위치 보간 (중앙 -> 패드구역)
        float Alpha = (float)i / (float)BridgeSteps;
        float BridgeY = FMath::Lerp(0.0f, PadZoneCenter, Alpha);

        FVector StepPos = BoxOrigin;
        StepPos.Z = PadZ;
        StepPos += FwdVec * StickOut;
        StepPos += RightVec * BridgeY;

        AActor* StepPlat = GetWorld()->SpawnActor<AActor>(PlatformClass, StepPos, PlatformRot);
        if (StepPlat) StepPlat->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);

        PadZ += FMath::RandRange(30.0f, 80.0f);
    }

    // 이제 패드 구역에 도착했으니 점프 패드 생성 시작
    float PadY = PadZoneCenter;
    int32 PadSkip = 0; // 바로 생성

    while (PadZ < BoxTopZ)
    {
        PadSkip--;
        if (PadSkip <= 0)
        {
            float RandomRange = (PadMaxY - PadMinY) * 0.3f;
            float RandomOffset = FMath::RandRange(-RandomRange, RandomRange);
            PadY = PadZoneCenter + RandomOffset;

            FVector PadPos = BoxOrigin;
            PadPos.Z = PadZ;
            PadPos += FwdVec * StickOut;
            PadPos += RightVec * PadY;

            AActor* NewPad = GetWorld()->SpawnActor<AActor>(JumpPadClass, PadPos, PlatformRot);
            if (NewPad) NewPad->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);

            PadSkip = FMath::RandRange(5, 7);
        }
        PadZ += GridHeight;
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