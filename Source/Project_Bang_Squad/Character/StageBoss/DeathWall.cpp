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
    // [설정값: 랜덤 & 공식]
    // ====================================================

    // 1. 일반 계단 (랜덤 구간에서 나옴)
    float MinStairGap = 220.0f;
    float MaxStairGap = 450.0f;

    // 2. 평지 롱 점프 (랜덤 구간 & 공식 시작점)
    // 요청하신 대로 560 ~ 760 사이 랜덤
    float MinLongFlat = 560.0f;
    float MaxLongFlat = 760.0f;

    // 3. 꼬리 계단 (공식: 턴 직전 따닥따닥)
    float BaseShortUp = 150.0f;

    // 4. 턴 점프
    float TurnJumpDist = 660.0f;

    // 5. 확률 (중간에 평지가 나올 확률 30%)
    float RandomFlatProb = 0.3f;

    float GridHeight = 110.0f;

    // ----------------------------------------------------

    float CurrentZ = BoxBottomZ + 50.0f;
    float CurrentY = 0.0f;
    float CurrentDirection = (FMath::RandBool()) ? 1.0f : -1.0f;
    int32 StepsInCurrentDir = 0;

    // [압축용 변수] 공간이 좁으면 이 값들이 줄어듭니다.
    float CurrentPatternLong = MaxLongFlat; // 공식용 롱 점프
    float CurrentPatternShort = BaseShortUp; // 공식용 숏 계단

    // 초기 설정
    int32 HeadLength = FMath::RandRange(1, 2); // 앞부분 랜덤 개수
    int32 TailLength = 4; // 최소 4개 시작
    int32 TargetSteps = HeadLength + 1 + TailLength; // 앞부분 + 롱점프1 + 뒷부분

    float MaxY = BoxExtent.Y - 100.0f;

    FVector StartPos = BoxOrigin;
    StartPos.Z = CurrentZ;
    StartPos += FwdVec * PlatformStickOut;
    GetWorld()->SpawnActor<AActor>(PlatformClass, StartPos, GetActorRotation())
        ->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);

    CurrentZ += GridHeight;

    while (CurrentZ < BoxTopZ)
    {
        // 1. 턴 여부 결정
        bool bIsJumpTurn = false;

        // 목표 개수 채웠으면 턴
        if (StepsInCurrentDir >= TargetSteps) bIsJumpTurn = true;

        // 벽 뚫기 방지 (현재 턴이 평지인지 위로인지에 따라 체크)
        // 안전하게 450 정도로 미리 체크
        float CheckNextY = CurrentY + (CurrentDirection * 450.0f);
        if (FMath::Abs(CheckNextY) > MaxY) bIsJumpTurn = true;

        if (bIsJumpTurn)
        {
            // [A. 턴 점프]
            CurrentDirection *= -1.0f;
            CurrentY += (CurrentDirection * TurnJumpDist);

            // 맵 밖 보정
            if (FMath::Abs(CurrentY) > MaxY)
            {
                CurrentY = (CurrentDirection > 0) ? -MaxY + 150.0f : MaxY - 150.0f;
            }

            // ====================================================
            // [B. 스마트 압축 (공식 패턴 보장용)]
            // ====================================================

            // 1. 남은 공간 계산
            float DistToWall = (MaxY)-(CurrentY * CurrentDirection);

            // 2. 필수 공간 계산 (공식: 롱 점프 1개 + 숏 계단 4개)
            // 앞부분(Head)은 짧게 잡아서 최소한의 공간만 계산
            float EssentialSpace = MinStairGap + MinLongFlat + (BaseShortUp * 4);

            // 3. 비율 계산 (공간 부족하면 축소)
            float ScaleRatio = 1.0f;
            if (DistToWall < EssentialSpace)
            {
                ScaleRatio = DistToWall / EssentialSpace;
                ScaleRatio = FMath::Max(ScaleRatio, 0.65f); // 너무 작아지지 않게 제한
            }

            // 4. 공식 패턴 간격 확정 (이번 줄 동안 유지)
            CurrentPatternLong = FMath::RandRange(MinLongFlat, MaxLongFlat) * ScaleRatio;
            CurrentPatternShort = BaseShortUp * ScaleRatio;

            // 5. 꼬리 개수 (공간 남으면 더 추가)
            float UsedSpace = MinStairGap + CurrentPatternLong + (CurrentPatternShort * 4);
            float RemainingSpace = DistToWall - UsedSpace;

            int32 ExtraTails = 0;
            if (RemainingSpace > 0)
            {
                ExtraTails = FMath::FloorToInt(RemainingSpace / CurrentPatternShort);
            }

            // 꼬리 길이: 최소 4개 ~ 최대 8개
            TailLength = 4 + ExtraTails;
            if (TailLength > 8) TailLength = 8;
            TailLength = FMath::RandRange(4, TailLength); // 랜덤성 부여

            // 앞부분 길이 (공간 여유에 따라)
            HeadLength = FMath::RandRange(1, 3);

            // 리셋
            StepsInCurrentDir = 0;
            TargetSteps = HeadLength + 1 + TailLength;
        }
        else
        {
            float GapToUse = 0.0f;
            bool bIsFlatJump = false;
            int32 RemSteps = TargetSteps - StepsInCurrentDir;

            // ==========================================
            // [Zone 1: 꼬리 구간 (공식)] -> 4개 이상 숏 계단
            // ==========================================
            if (RemSteps <= TailLength)
            {
                GapToUse = CurrentPatternShort; // 150 (압축됨)
                bIsFlatJump = false; // 위로
            }
            // ==========================================
            // [Zone 2: 꼬리 직전 (공식)] -> 평지 롱 점프
            // ==========================================
            else if (RemSteps == TailLength + 1)
            {
                GapToUse = CurrentPatternLong; // 560~760 (압축됨)
                bIsFlatJump = true; // ★ 평지
            }
            // ==========================================
            // [Zone 3: 앞부분 (랜덤 구간)]
            // ==========================================
            else
            {
                // 1. 턴 직후 첫 발판은 무조건 위로 (안전빵)
                if (StepsInCurrentDir == 0)
                {
                    GapToUse = FMath::RandRange(MinStairGap, MaxStairGap);
                    bIsFlatJump = false;
                }
                // 2. 그 외 중간 발판들 (랜덤)
                else
                {
                    // 30% 확률로 평지 롱 점프, 아니면 일반 계단
                    bool bTryFlat = (FMath::RandRange(0.0f, 1.0f) < RandomFlatProb);

                    if (bTryFlat)
                    {
                        // 평지 (560 ~ 760)
                        GapToUse = FMath::RandRange(MinLongFlat, MaxLongFlat);

                        // ★중요★ 여기서 벽 뚫으면 안됨 (뒤에 공식 패턴 나와야 함)
                        // 대략적인 공간 체크
                        float PredictY = CurrentY + (CurrentDirection * (GapToUse + CurrentPatternLong + (CurrentPatternShort * 4)));

                        if (FMath::Abs(PredictY) < MaxY)
                        {
                            bIsFlatJump = true; // 평지 확정
                        }
                        else
                        {
                            // 공간 부족하면 일반 계단으로 변경
                            GapToUse = FMath::RandRange(MinStairGap, MaxStairGap);
                            bIsFlatJump = false;
                        }
                    }
                    else
                    {
                        // 일반 계단 (220 ~ 450)
                        GapToUse = FMath::RandRange(MinStairGap, MaxStairGap);
                        bIsFlatJump = false; // 위로
                    }
                }
            }

            // 위치 적용
            CurrentY += (CurrentDirection * GapToUse);

            if (bIsFlatJump) CurrentZ -= GridHeight;

            // 벽 보정 (최종 안전장치)
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