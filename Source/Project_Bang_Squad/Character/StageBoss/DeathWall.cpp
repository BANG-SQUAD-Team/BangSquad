#include "Project_Bang_Squad/Character/StageBoss/DeathWall.h"
#include "Project_Bang_Squad/BossPattern/Boss1_Rampart.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"

ADeathWall::ADeathWall()
{
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

    // 박스 기본 크기
    SpawnVolume->SetBoxExtent(FVector(100.0f, 1000.0f, 1000.0f));

    // 3. [New] 이동 방향 화살표 (빨간 화살표가 이동 방향임)
    DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
    DirectionArrow->SetupAttachment(RootComponent);
    DirectionArrow->ArrowColor = FColor::Yellow;
    DirectionArrow->ArrowSize = 5.0f;
    // 기본적으로 Right(Y축) 방향을 가리키게 설정 (기존 코드 존중)
    DirectionArrow->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));

    // [설정값 초기화]
    FirstPlatformHeight = 60.0f;
    LayerHeight = 150.0f;
    BranchWidth = 120.0f;
    PlatformStickOut = 150.0f;
    BranchProbability = 0.6f;
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

        // 성벽 충돌 무시 로직
        // 성벽(Rampart)과 충돌 무시 설정
        TArray<AActor*> FoundRamparts;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABoss1_Rampart::StaticClass(), FoundRamparts);

        // (Rampart 관련 로직 생략되어 있다면 여기에 추가 필요)
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

        AddActorWorldOffset(MoveDir * MoveSpeed * DeltaTime, false);
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

    // ====================================================
    // [설정값]
    // ====================================================
    float GridHeight = 110.0f;
    float StickOut = 200.0f;

    // 변수 초기화 (HEAD 부분에 있던 로직 복원)
    float CurrentZ = BoxBottomZ + 50.0f;
    bool bStoneOnLeft = (FMath::RandBool());
    float StoneMainDir = bStoneOnLeft ? 1.0f : -1.0f;
    float StoneWanderDir = StoneMainDir;
    float StoneY = StoneMainDir * 200.0f;
    int32 StoneSteps = 0;

    // ----------------------------------------------------
    // [1] 공통 구간 (2개 생성 - 확실한 계단 만들기)
    // ----------------------------------------------------
    // 1번 발판
    FVector Pos1 = BoxOrigin;
    Pos1.Z = CurrentZ;
    Pos1 += FwdVec * StickOut;
    Pos1 += RightVec * 0.0f;

    AActor* NewPlat1 = GetWorld()->SpawnActor<AActor>(PlatformClass, Pos1, GetActorRotation());
    if (NewPlat1) NewPlat1->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);

    // 2번 발판 (분기점) - 옆으로 이동
    CurrentZ += GridHeight;
    float ForkY = StoneMainDir * 200.0f;

    FVector Pos2 = BoxOrigin;
    Pos2.Z = CurrentZ;
    Pos2 += FwdVec * StickOut;
    Pos2 += RightVec * ForkY;

    AActor* NewPlat2 = GetWorld()->SpawnActor<AActor>(PlatformClass, Pos2, GetActorRotation());
    if (NewPlat2) NewPlat2->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);

    // 돌계단(Stone) 관련 변수 업데이트
    float StoneZ = CurrentZ + GridHeight;
    int32 StoneTarget = 4;

    while (StoneZ < BoxTopZ)
    {
        if (FMath::Abs(StoneY + (StoneWanderDir * 300.0f)) > MaxY)
            StoneWanderDir *= -1.0f;

        if (StoneSteps >= StoneTarget) {
            StoneWanderDir *= -1.0f;
            StoneSteps = 0;
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