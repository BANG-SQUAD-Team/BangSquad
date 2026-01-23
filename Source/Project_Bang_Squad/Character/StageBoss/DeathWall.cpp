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
    float MinStairGap = 220.0f;
    float MaxStairGap = 450.0f;
    float MinLongFlat = 560.0f;
    float MaxLongFlat = 760.0f;
    float BaseShortUp = 150.0f;     // 꼬리 계단 간격
    float TurnJumpDist = 660.0f;    // 턴 점프 기본 거리

    // ★ 꼬리 필수 개수 (4개)
    int32 MinTailCount = 4;
    // ★ 꼬리를 위해 확보해야 할 필수 공간 (4개 * 150 + 여유분 50)
    float EssentialSpace = (BaseShortUp * MinTailCount) + 50.0f;

    float GridHeight = 110.0f;

    // ----------------------------------------------------

    float CurrentZ = BoxBottomZ + 50.0f;
    float CurrentY = 0.0f;
    float CurrentDirection = (FMath::RandBool()) ? 1.0f : -1.0f;
    int32 StepsInCurrentDir = 0;

    float CurrentPatternLong = MaxLongFlat;
    float CurrentPatternShort = BaseShortUp;

    int32 HeadLength = FMath::RandRange(1, 2);
    int32 TailLength = MinTailCount;
    int32 TargetSteps = HeadLength + 1 + TailLength;

    float MaxY = BoxExtent.Y - 100.0f; // 벽 한계점

    // 첫 발판
    FVector StartPos = BoxOrigin;
    StartPos.Z = CurrentZ;
    StartPos += FwdVec * PlatformStickOut;
    GetWorld()->SpawnActor<AActor>(PlatformClass, StartPos, GetActorRotation())
        ->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);

    CurrentZ += GridHeight;

    while (CurrentZ < BoxTopZ)
    {
        bool bIsJumpTurn = false;

        // 1. 목표 개수 채웠으면 턴
        if (StepsInCurrentDir >= TargetSteps) bIsJumpTurn = true;

        // 2. 벽 뚫기 방지 (미리 체크)
        float CheckNextY = CurrentY + (CurrentDirection * 450.0f);
        if (FMath::Abs(CheckNextY) > MaxY) bIsJumpTurn = true;

        if (bIsJumpTurn)
        {
            // ==========================================
            // [A. 턴 점프 로직 (강제 공간 확보)]
            // ==========================================
            CurrentDirection *= -1.0f; // 방향 전환

            // 원래 가려던 목표 위치
            float IdealY = CurrentY + (CurrentDirection * TurnJumpDist);

            // ★ [핵심] 벽에서 역산한 "안전 한계선" 계산
            // 예: MaxY가 1000이고, 필수 공간이 650이면, 안전 한계선은 350
            // 즉, 350보다 더 멀리 가면 4개가 안 들어가므로 350에서 멈춰야 함.
            float SafeLimitY = MaxY - EssentialSpace;

            // 착지 위치가 안전 한계선을 넘어가면 강제로 당김
            if (FMath::Abs(IdealY) > SafeLimitY)
            {
                // 방향에 맞춰서 최대치로 제한 (Make it possible!)
                CurrentY = (CurrentDirection > 0) ? SafeLimitY : -SafeLimitY;
            }
            else
            {
                // 안전하면 원래대로 이동
                CurrentY = IdealY;
            }

            // ----------------------------------------------------
            // 여기부터는 이제 공간이 "확보된 상태"이므로 안심하고 패턴 생성
            // ----------------------------------------------------

            // 남은 공간 재계산 (이제 무조건 EssentialSpace 이상임)
            float DistToWall = (MaxY)-(CurrentY * CurrentDirection);

            // 남은 공간을 꼬리 개수만큼 쪼개서 비율을 정하지 않고,
            // 그냥 꼬리는 정해진 간격(Short)으로 채우고 남는 건 덤으로 처리

            // 공식 패턴 간격 설정
            CurrentPatternLong = FMath::RandRange(MinLongFlat, MaxLongFlat);
            CurrentPatternShort = BaseShortUp; // 150 고정

            // 공간에 꼬리가 몇 개 더 들어가는지 계산
            // (이미 4개 공간은 확보했으니 Extra만 계산)
            float UsedSpaceForMinTails = CurrentPatternShort * MinTailCount;
            float RemainingAfterMinTails = DistToWall - UsedSpaceForMinTails;

            int32 ExtraTails = 0;
            if (RemainingAfterMinTails > 0)
            {
                // 남는 공간에도 촘촘하게 박기
                ExtraTails = FMath::FloorToInt(RemainingAfterMinTails / CurrentPatternShort);
            }

            // 꼬리 길이 확정 (최소 4개 + 알파)
            TailLength = MinTailCount + ExtraTails;

            // 너무 길어지면(8개 초과) 지루하니까 자르고 랜덤성 부여
            if (TailLength > 8) TailLength = 8;

            // 앞부분 길이
            HeadLength = FMath::RandRange(1, 3);

            // 리셋
            StepsInCurrentDir = 0;
            TargetSteps = HeadLength + 1 + TailLength;
        }
        else
        {
            // ==========================================
            // [B. 직진 로직]
            // ==========================================
            float GapToUse = 0.0f;
            bool bIsFlatJump = false;
            int32 RemSteps = TargetSteps - StepsInCurrentDir;

            // [Zone 1: 꼬리 구간] -> 무조건 짧게 위로
            if (RemSteps <= TailLength)
            {
                GapToUse = CurrentPatternShort;
                bIsFlatJump = false;
            }
            // [Zone 2: 꼬리 직전] -> 평지 롱 점프
            else if (RemSteps == TailLength + 1)
            {
                GapToUse = CurrentPatternLong;
                bIsFlatJump = true;
            }
            // [Zone 3: 앞부분] -> 랜덤
            else
            {
                if (StepsInCurrentDir == 0) // 턴 직후 첫 발판
                {
                    GapToUse = FMath::RandRange(MinStairGap, MaxStairGap);
                    bIsFlatJump = false;
                }
                else
                {
                    // 30% 확률로 평지 롱 점프 시도
                    // (헤더에 선언된 변수 사용 안 하고 직접 값 넣음)
                    bool bTryFlat = (FMath::RandRange(0.0f, 1.0f) < 0.3f);

                    if (bTryFlat)
                    {
                        GapToUse = FMath::RandRange(MinLongFlat, MaxLongFlat);

                        // ★ 앞부분 롱점프가 너무 멀리가서 뒤에 꼬리 공간 침범하는지 체크
                        float FutureNeeded = (CurrentPatternShort * MinTailCount) + CurrentPatternLong;
                        float PredictY = CurrentY + (CurrentDirection * (GapToUse + FutureNeeded));

                        if (FMath::Abs(PredictY) < MaxY)
                        {
                            bIsFlatJump = true;
                        }
                        else
                        {
                            GapToUse = FMath::RandRange(MinStairGap, MaxStairGap);
                            bIsFlatJump = false;
                        }
                    }
                    else
                    {
                        GapToUse = FMath::RandRange(MinStairGap, MaxStairGap);
                        bIsFlatJump = false;
                    }
                }
            }

            // 위치 적용
            CurrentY += (CurrentDirection * GapToUse);

            if (bIsFlatJump) CurrentZ -= GridHeight;

            // 혹시라도 벽 뚫으면 보정 (최후의 안전장치)
            if (FMath::Abs(CurrentY) > MaxY)
            {
                CurrentY = (CurrentDirection > 0) ? MaxY - 30.0f : -MaxY + 30.0f;
            }
        }

        // 생성
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

        CurrentZ += GridHeight;
        StepsInCurrentDir++;
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