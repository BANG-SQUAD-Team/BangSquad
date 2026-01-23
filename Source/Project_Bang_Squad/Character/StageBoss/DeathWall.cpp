#include "Project_Bang_Squad/Character/StageBoss/DeathWall.h"
#include "Project_Bang_Squad/BossPattern/Boss1_Rampart.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"

ADeathWall::ADeathWall()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);

    // 1. 벽 메쉬
    WallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallMesh"));
    RootComponent = WallMesh;
    WallMesh->SetCollisionProfileName(TEXT("BlockAll")); // 플레이어를 밀어내기 위함

    // 2. 생성 범위 박스
    SpawnVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnVolume"));
    SpawnVolume->SetupAttachment(RootComponent);
    SpawnVolume->SetLineThickness(5.0f);
    SpawnVolume->ShapeColor = FColor::Red;
    SpawnVolume->SetCollisionProfileName(TEXT("NoCollision"));
    SpawnVolume->SetBoxExtent(FVector(100.0f, 1000.0f, 1000.0f));

    // 3. [New] 이동 방향 화살표 (빨간 화살표가 이동 방향임)
    DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
    DirectionArrow->SetupAttachment(RootComponent);
    DirectionArrow->ArrowColor = FColor::Yellow;
    DirectionArrow->ArrowSize = 5.0f;
    // 기본적으로 Right(Y축) 방향을 가리키게 설정 (기존 코드 존중)
    DirectionArrow->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));

    // [설정값 초기화]
    PlatformStickOut = 150.0f;
}

void ADeathWall::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ADeathWall, bIsActive);
}

void ADeathWall::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        GeneratePlatforms();

        // 성벽(Rampart)과 충돌 무시 설정
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

    // 서버에서만 이동 처리 -> 클라이언트는 ReplicateMovement로 위치 동기화됨
    if (HasAuthority() && bIsActive)
    {
        // [수정] 화살표 컴포넌트가 가리키는 방향으로 이동 (직관적)
        FVector MoveDir = DirectionArrow->GetForwardVector();

        // bSweep=false: 벽이 플레이어를 뚫고 지나가려 할 때, 
        // 물리 엔진이 플레이어를 밀어내도록(Depenetrate) 유도합니다.
        AddActorWorldOffset(MoveDir * MoveSpeed * DeltaTime, false);
    }
}

void ADeathWall::SpawnPlatformAt(FVector Location)
{
    if (!PlatformClass) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* NewPlatform = GetWorld()->SpawnActor<AActor>(PlatformClass, Location, GetActorRotation(), SpawnParams);
    if (NewPlatform)
    {
        // [중요] 벽의 자식으로 붙여서 벽과 함께 이동하도록 설정
        NewPlatform->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
    }
}

void ADeathWall::GeneratePlatforms()
{
    if (!PlatformClass || !SpawnVolume) return;

    // 박스 정보 가져오기
    FVector BoxExtent = SpawnVolume->GetScaledBoxExtent();
    FVector BoxOrigin = SpawnVolume->GetComponentLocation();

    // 방향 벡터 (월드 기준)
    FVector FwdVec = SpawnVolume->GetForwardVector(); // 벽이 튀어나온 방향
    FVector RightVec = SpawnVolume->GetRightVector(); // 벽의 너비 방향

    float BoxBottomZ = BoxOrigin.Z - BoxExtent.Z;
    float BoxTopZ = BoxOrigin.Z + BoxExtent.Z;
    float MaxY = BoxExtent.Y - 100.0f; // 안전 마진

    // === 알고리즘 설정 ===
    float MinStairGap = 220.0f;
    float MaxStairGap = 450.0f;
    float MinLongFlat = 560.0f;
    float MaxLongFlat = 760.0f;
    float BaseShortUp = 150.0f;
    float GridHeight = 110.0f;

    // 꼬리 패턴 변수
    int32 MinTailCount = 4;
    float TurnJumpDist = 660.0f;
    float EssentialSpace = (BaseShortUp * MinTailCount) + 50.0f;

    // === 상태 변수 ===
    float CurrentZ = BoxBottomZ + 50.0f;
    float CurrentY = 0.0f; // 중심에서 시작
    float CurrentDirection = (FMath::RandBool()) ? 1.0f : -1.0f;
    int32 StepsInCurrentDir = 0;

    int32 HeadLength = FMath::RandRange(1, 2);
    int32 TailLength = MinTailCount;
    int32 TargetSteps = HeadLength + 1 + TailLength;

    // [첫 발판 생성]
    FVector StartPos = BoxOrigin;
    StartPos.Z = CurrentZ;
    StartPos += FwdVec * PlatformStickOut; // 벽에서 튀어나오게
    SpawnPlatformAt(StartPos);

    CurrentZ += GridHeight;

    // [패턴 생성 루프]
    while (CurrentZ < BoxTopZ)
    {
        bool bIsJumpTurn = false;

        // 방향 전환 조건 체크
        float CheckNextY = CurrentY + (CurrentDirection * 450.0f);
        if (StepsInCurrentDir >= TargetSteps || FMath::Abs(CheckNextY) > MaxY)
        {
            bIsJumpTurn = true;
        }

        float GapToUse = 0.0f;
        bool bIsFlatJump = false; // 높이 변화 없는 점프 여부

        if (bIsJumpTurn)
        {
            // --- [턴 점프 로직] ---
            CurrentDirection *= -1.0f; // 방향 반전

            float IdealY = CurrentY + (CurrentDirection * TurnJumpDist);
            float SafeLimitY = MaxY - EssentialSpace;

            // 벽 뚫기 방지 보정
            if (FMath::Abs(IdealY) > SafeLimitY)
            {
                CurrentY = (CurrentDirection > 0) ? SafeLimitY : -SafeLimitY;
            }
            else
            {
                CurrentY = IdealY;
            }

            // 다음 패턴 길이 재설정 (남은 공간 계산)
            float DistToWall = MaxY - (CurrentY * CurrentDirection); // 현재 진행 방향 쪽 남은 거리
            float RemainingSpace = DistToWall - (BaseShortUp * MinTailCount);

            int32 ExtraTails = (RemainingSpace > 0) ? FMath::FloorToInt(RemainingSpace / BaseShortUp) : 0;
            TailLength = FMath::Clamp(MinTailCount + ExtraTails, 4, 8);
            HeadLength = FMath::RandRange(1, 3);

            StepsInCurrentDir = 0;
            TargetSteps = HeadLength + 1 + TailLength;
        }
        else
        {
            // --- [직진 로직] ---
            int32 RemSteps = TargetSteps - StepsInCurrentDir;

            if (RemSteps <= TailLength) // 꼬리 구간 (짧은 점프)
            {
                GapToUse = BaseShortUp;
                bIsFlatJump = false;
            }
            else if (RemSteps == TailLength + 1) // 꼬리 직전 (롱 점프)
            {
                GapToUse = FMath::RandRange(MinLongFlat, MaxLongFlat);
                bIsFlatJump = true;
            }
            else // 머리 구간 (랜덤)
            {
                // 첫 발판이 아니고, 확률적으로 평지 점프
                if (StepsInCurrentDir > 0 && FMath::RandRange(0.0f, 1.0f) < 0.3f)
                {
                    GapToUse = FMath::RandRange(MinLongFlat, MaxLongFlat);
                    // 미래 위치 예측해서 벽 안 뚫으면 롱점프 허용
                    if (FMath::Abs(CurrentY + (CurrentDirection * (GapToUse + 1000.0f))) < MaxY)
                    {
                        bIsFlatJump = true;
                    }
                    else
                    {
                        GapToUse = FMath::RandRange(MinStairGap, MaxStairGap);
                    }
                }
                else
                {
                    GapToUse = FMath::RandRange(MinStairGap, MaxStairGap);
                }
            }

            CurrentY += (CurrentDirection * GapToUse);

            // 높이 보정 (평지 점프면 Z축 상승 취소)
            if (bIsFlatJump) CurrentZ -= GridHeight;

            // 최종 안전장치
            if (FMath::Abs(CurrentY) > MaxY)
            {
                CurrentY = (CurrentDirection > 0) ? MaxY - 30.0f : -MaxY + 30.0f;
            }
        }

        // [최종 위치 계산 및 스폰]
        FVector SpawnPos = BoxOrigin;
        SpawnPos.Z = CurrentZ;
        SpawnPos += FwdVec * PlatformStickOut;
        SpawnPos += RightVec * CurrentY;

        SpawnPlatformAt(SpawnPos);

        // 다음 스텝 준비
        CurrentZ += GridHeight;
        StepsInCurrentDir++;
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