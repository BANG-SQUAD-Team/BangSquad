#include "DeathWall.h"
#include "Kismet/KismetMathLibrary.h" 
#include "Kismet/GameplayStatics.h"
#include "Project_Bang_Squad/BossPattern/Boss1_Rampart.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"

ADeathWall::ADeathWall()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);

    // [1] 트리거 박스 생성 (이벤트 감지용 대장)
    KillZoneTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("KillZoneTrigger"));
    RootComponent = KillZoneTrigger;

    // ★ 트리거 박스 콜리전 설정 (여기가 핵심)
    KillZoneTrigger->SetBoxExtent(FVector(50.0f, 1000.0f, 1000.0f));
    KillZoneTrigger->SetCollisionProfileName(TEXT("Custom"));
    KillZoneTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly); // 물리 충돌 없음, 감지(Query)만 함
    KillZoneTrigger->SetCollisionResponseToAllChannels(ECR_Ignore); // 일단 다 무시하고
    KillZoneTrigger->SetCollisionResponseToChannel(ECC_KillZone, ECR_Overlap); // ★ 킬존만 Overlap 감지!
    KillZoneTrigger->SetGenerateOverlapEvents(true); // 이벤트 켜기

    // [2] 벽 메쉬 (보이는 용도 + 플레이어 밀기용)
    WallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallMesh"));
    WallMesh->SetupAttachment(KillZoneTrigger);
    WallMesh->SetCollisionProfileName(TEXT("BlockAll")); // 얘는 플레이어를 밀어야 하니 Block

    // 메쉬 회전값 초기화
    MeshRotation = FRotator(0.0f, -90.0f, 0.0f);
    WallMesh->SetRelativeRotation(MeshRotation);

    // [3] 스폰 볼륨
    SpawnVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnVolume"));
    SpawnVolume->SetupAttachment(KillZoneTrigger);
    SpawnVolume->SetLineThickness(5.0f);
    SpawnVolume->ShapeColor = FColor::Red;
    SpawnVolume->SetCollisionProfileName(TEXT("NoCollision"));
    SpawnVolume->SetBoxExtent(FVector(100.0f, 1000.0f, 1000.0f));

    FirstPlatformHeight = 60.0f;
    LayerHeight = 150.0f;
    BranchWidth = 120.0f;
    PlatformStickOut = 150.0f;
    BranchProbability = 0.6f;
}

void ADeathWall::BeginPlay()
{
    Super::BeginPlay();

    if (WallMesh)
    {
        WallMesh->SetRelativeRotation(MeshRotation);
    }

    if (HasAuthority())
    {
        GeneratePlatforms();
        SetLifeSpan(105.0f);

        TArray<AActor*> FoundRamparts;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABoss1_Rampart::StaticClass(), FoundRamparts);
        for (AActor* Actor : FoundRamparts)
        {
            if (ABoss1_Rampart* Rampart = Cast<ABoss1_Rampart>(Actor))
            {
                if (KillZoneTrigger) KillZoneTrigger->IgnoreActorWhenMoving(Rampart, true);
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

void ADeathWall::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    // 저장해둔 자식들(발판, 점프패드)을 전부 파괴합니다.
    for (AActor* ChildActor : SpawnedActors)
    {
        // 아직 살아있는 놈들만 골라서 파괴
        if (ChildActor && !ChildActor->IsPendingKillPending())
        {
            ChildActor->Destroy();
        }
    }

    // 리스트 비우기
    SpawnedActors.Empty();
}

void ADeathWall::GeneratePlatforms()
{
    if (!PlatformClass || !JumpPadClass) return;

    // ====================================================
    // 1. 기본 설정 (80% vs 20%) -> 님 로직 그대로
    // ====================================================
    FVector BoxExtent = SpawnVolume->GetScaledBoxExtent();
    FVector BoxOrigin = SpawnVolume->GetComponentLocation();
    FVector FwdVec = SpawnVolume->GetForwardVector();
    FVector RightVec = SpawnVolume->GetRightVector();

    float BoxBottomZ = BoxOrigin.Z - BoxExtent.Z;
    float BoxTopZ = BoxOrigin.Z + BoxExtent.Z;
    float TotalWidth = BoxExtent.Y * 2.0f;
    float PadZoneWidth = TotalWidth * 0.2f;

    FRotator BaseRot = GetActorRotation();
    FRotator PlatformRot = BaseRot;
    PlatformRot.Yaw += 90.0f;

    bool bStoneOnLeft = FMath::RandBool();

    float LeftWall = -BoxExtent.Y + 50.0f;
    float RightWall = BoxExtent.Y - 50.0f;

    float DividerY = 0.0f;
    float StoneMinY, StoneMaxY, PadMinY, PadMaxY;
    float StoneDir = 0.0f;
    float PadDir = 0.0f;

    if (bStoneOnLeft)
    {
        StoneDir = -1.0f;
        PadDir = 1.0f;
        DividerY = RightWall - PadZoneWidth;
        StoneMinY = LeftWall;
        StoneMaxY = DividerY - 50.0f;
        PadMinY = DividerY + 50.0f;
        PadMaxY = RightWall;
    }
    else
    {
        StoneDir = 1.0f;
        PadDir = -1.0f;
        DividerY = LeftWall + PadZoneWidth;
        PadMinY = LeftWall;
        PadMaxY = DividerY - 50.0f;
        StoneMinY = DividerY + 50.0f;
        StoneMaxY = RightWall;
    }

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

    // ----------------------------------------------------
    // [2] 공통 시작 구간 (Start)
    // ----------------------------------------------------
    float CurrentZ = BoxBottomZ + 20.0f;

    // 1번 발판
    FVector Pos1 = BoxOrigin;
    Pos1.Z = CurrentZ;
    Pos1 += FwdVec * StickOut;

    // [수정됨] 변수에 담고 리스트에 추가
    AActor* P1 = GetWorld()->SpawnActor<AActor>(PlatformClass, Pos1, PlatformRot);
    if (P1)
    {
        P1->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
        SpawnedActors.Add(P1); // ★ 추가
    }

    // 2번 발판
    float StoneStartZ = CurrentZ + GridHeight;
    float StartY = StoneDir * 200.0f;

    FVector Pos2 = BoxOrigin;
    Pos2.Z = StoneStartZ;
    Pos2 += FwdVec * StickOut;
    Pos2 += RightVec * StartY;

    // [수정됨] 변수에 담고 리스트에 추가
    AActor* P2 = GetWorld()->SpawnActor<AActor>(PlatformClass, Pos2, PlatformRot);
    if (P2)
    {
        P2->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
        SpawnedActors.Add(P2); // ★ 추가
    }

    float ForkZ = StoneStartZ;

    // ====================================================
    // [3] 돌 발판 루프 -> 로직 그대로
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

        // [수정됨] 변수에 담고 리스트에 추가
        AActor* NewPlat = GetWorld()->SpawnActor<AActor>(PlatformClass, SpawnPos, PlatformRot);
        if (NewPlat)
        {
            NewPlat->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
            SpawnedActors.Add(NewPlat); // ★ 추가
        }

        StoneZ += GridHeight;
        StepsInCurrentDir++;
    }

    // ====================================================
    // [4] 점프 패드 루프 -> 로직 그대로
    // ====================================================
    float PadZ = CurrentZ + 20.0f;
    float PadZoneCenter = (PadMinY + PadMaxY) * 0.5f;

    int32 BridgeSteps = 4;

    for (int32 i = 1; i <= BridgeSteps; ++i)
    {
        float Alpha = (float)i / (float)BridgeSteps;
        float BridgeY = FMath::Lerp(0.0f, PadZoneCenter, Alpha);

        FVector StepPos = BoxOrigin;
        StepPos.Z = PadZ;
        StepPos += FwdVec * StickOut;
        StepPos += RightVec * BridgeY;

        // [수정됨] 변수에 담고 리스트에 추가
        AActor* StepPlat = GetWorld()->SpawnActor<AActor>(PlatformClass, StepPos, PlatformRot);
        if (StepPlat)
        {
            StepPlat->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
            SpawnedActors.Add(StepPlat); // ★ 추가
        }

        PadZ += FMath::RandRange(30.0f, 80.0f);
    }

    float PadY = PadZoneCenter;
    int32 PadSkip = 0;

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

            // [수정됨] 변수에 담고 리스트에 추가
            AActor* NewPad = GetWorld()->SpawnActor<AActor>(JumpPadClass, PadPos, PlatformRot);
            if (NewPad)
            {
                NewPad->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
                SpawnedActors.Add(NewPad); // ★ 추가
            }

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

void ADeathWall::NotifyActorBeginOverlap(AActor* OtherActor)
{
    // [로그 1] 일단 부딪히긴 했는지 확인 (이게 안 뜨면 설정 문제)
    if (OtherActor)
    {
        UE_LOG(LogTemp, Error, TEXT(">>> [1] EVENT FIRED! Something hit me: %s"), *OtherActor->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT(">>> [1] EVENT FIRED! But OtherActor is NULL"));
    }

    Super::NotifyActorBeginOverlap(OtherActor);

    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT(">>> [2] Client ignored. (Server only)"));
        return;
    }

    if (!OtherActor) return;

    UPrimitiveComponent* OtherRoot = Cast<UPrimitiveComponent>(OtherActor->GetRootComponent());
    if (!OtherRoot)
    {
        UE_LOG(LogTemp, Error, TEXT(">>> [3] FAIL: OtherActor has no RootComponent."));
        return;
    }

    // 채널 확인
    ECollisionChannel OtherChannel = OtherRoot->GetCollisionObjectType();
    UE_LOG(LogTemp, Warning, TEXT(">>> [4] Hit Channel Info -> Name: %s / Channel Number: %d"), *OtherActor->GetName(), (int32)OtherChannel);

    // 우리가 찾는 KillZone(3번)인지 확인
    if (OtherChannel == ECC_KillZone)
    {
        UE_LOG(LogTemp, Error, TEXT(">>> [5] MATCH! KillZone Detected. DESTROYING WALL."));
        Destroy();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT(">>> [5] MISMATCH: We hit something, but it's not KillZone."));
    }
}