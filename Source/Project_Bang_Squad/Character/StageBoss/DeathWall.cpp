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

    // 리듬 패턴 관련 변수 초기화 (헤더에 없어도 여기서 쓰기 위해 로컬변수로 사용하거나 하드코딩)
    // *GeneratePlatforms 함수 내에서 설정됩니다*
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
    // [설정: 리듬 액션 - 넓게, 좁게, 점프!]
    // ====================================================

    // 1. 평소 계단 (220 ~ 450 랜덤)
    float MinStairGap = 220.0f;
    float MaxStairGap = 450.0f;

    // 2. ★ 리듬 패턴 설정 ★
    // "턴하기 직전 패드는 220, 그 전 패드는 450"
    float LastStepGap = 220.0f;      // 마지막 발판 (도움닫기/브레이크)
    float SecondLastStepGap = 450.0f; // 그 전 발판 (멀리뛰기)

    // 3. 턴 점프 (600 ~ 650 랜덤)
    float MinJumpGap = 600.0f;
    float MaxJumpGap = 650.0f;

    // 높이: 110cm
    float GridHeight = 110.0f;

    // ----------------------------------------------------

    // [1. 바닥]
    float CurrentZ = BoxBottomZ + 50.0f;
    float CurrentY = 0.0f;
    float CurrentDirection = (FMath::RandBool()) ? 1.0f : -1.0f;
    int32 StepsInCurrentDir = 0;

    // 몇 층까지 가고 꺾을까? (최소 4칸은 가야 패턴이 나옴)
    int32 TargetSteps = FMath::RandRange(4, 6);

    // 첫 발판
    FVector StartPos = BoxOrigin;
    StartPos.Z = CurrentZ;
    StartPos += FwdVec * PlatformStickOut;
    GetWorld()->SpawnActor<AActor>(PlatformClass, StartPos, GetActorRotation())
        ->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);

    // [2. 패턴 생성]
    CurrentZ += GridHeight;
    float MaxY = BoxExtent.Y - 100.0f;

    while (CurrentZ < BoxTopZ)
    {
        // 1. 행동 결정
        bool bIsJumpTurn = false;

        // 목표 층수 채웠거나, 벽 뚫으면 턴
        if (StepsInCurrentDir >= TargetSteps) bIsJumpTurn = true;

        // 벽 체크 (대략 450으로 계산)
        float NextStepY = CurrentY + (CurrentDirection * 450.0f);
        if (FMath::Abs(NextStepY) > MaxY) bIsJumpTurn = true;

        // 2. 위치 계산
        if (bIsJumpTurn)
        {
            // [점프 턴] 600 ~ 650
            float RandomJump = FMath::RandRange(MinJumpGap, MaxJumpGap);

            CurrentDirection *= -1.0f;
            CurrentY += (CurrentDirection * RandomJump);

            // 맵 보정
            if (FMath::Abs(CurrentY) > MaxY)
            {
                CurrentY = (CurrentDirection > 0) ? -MaxY + 100.0f : MaxY - 100.0f;
            }

            StepsInCurrentDir = 0;
            TargetSteps = FMath::RandRange(4, 6);
        }
        else
        {
            // [계단 구간]
            float GapToUse;

            // ★ 사용자 요청 패턴 적용 ★
            if (StepsInCurrentDir == TargetSteps - 1)
            {
                // [마지막 발판] 턴하기 직전 -> 220 (짧게)
                GapToUse = LastStepGap;
            }
            else if (StepsInCurrentDir == TargetSteps - 2)
            {
                // [마지막 전 발판] 그 전 패드 -> 450 (길게)
                GapToUse = SecondLastStepGap;
            }
            else
            {
                // [평소] 220 ~ 450 랜덤
                GapToUse = FMath::RandRange(MinStairGap, MaxStairGap);
            }

            CurrentY += (CurrentDirection * GapToUse);
        }

        // 3. 생성
        FVector SpawnPos = BoxOrigin;
        SpawnPos.Z = CurrentZ;
        SpawnPos += FwdVec * PlatformStickOut;
        SpawnPos += RightVec * CurrentY;

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;

        AActor* NewPlatform = GetWorld()->SpawnActor<AActor>(PlatformClass, SpawnPos, GetActorRotation(), SpawnParams);
        if (NewPlatform)
        {
            NewPlatform->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
        }

        // 4. 업데이트
        CurrentZ += GridHeight;
        StepsInCurrentDir++;
    }
}

// 헤더에 선언되어 있으므로 구현체만 남겨둠 (현재 로직에서는 직접 호출되지 않더라도 에러 방지용)
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

void ADeathWall::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (HasAuthority() && bIsActive)
    {
        AddActorWorldOffset(GetActorForwardVector() * MoveSpeed * DeltaTime);
    }
}

void ADeathWall::ActivateWall()
{
    if (HasAuthority()) bIsActive = true;
}