#include "Project_Bang_Squad/Character/Player/Mage/MovePillar.h"
#include "Net/UnrealNetwork.h"
#include "Curves/CurveFloat.h"
#include "Components/StaticMeshComponent.h"

AMovePillar::AMovePillar()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    // [수정 2] 움직일 부모 컴포넌트 생성
    // 블루프린트에서 이 컴포넌트 아래에 기둥 메쉬들을 여러 개 붙이세요.
    MovingPlatformRoot = CreateDefaultSubobject<USceneComponent>(TEXT("MovingPlatformRoot"));
    MovingPlatformRoot->SetupAttachment(SceneRoot);

    TargetLocationOffset = FVector(0.f, 0.f, 400.f);
}

void AMovePillar::BeginPlay()
{
    Super::BeginPlay();

    // 움직임의 기준은 MovingPlatformRoot의 시작 위치
    if (MovingPlatformRoot)
    {
        StartLocation = MovingPlatformRoot->GetRelativeLocation();
        TargetLocation = StartLocation + TargetLocationOffset;
    }
}

void AMovePillar::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AMovePillar, bIsActive);
}

void AMovePillar::SetMageHighlight(bool bActive)
{
    TArray<USceneComponent*> ChildComps;

    if (MovingPlatformRoot)
    {
        // true: 자식의 자식까지 모두 찾음 (Recursive)
        MovingPlatformRoot->GetChildrenComponents(true, ChildComps);
    }

    for (USceneComponent* Comp : ChildComps)
    {
        if (UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Comp))
        {
            Mesh->SetRenderCustomDepth(bActive);
            Mesh->SetCustomDepthStencilValue(bActive ? 250 : 0);
        }
    }
}

void AMovePillar::ProcessMageInput(FVector Direction)
{
    if (!HasAuthority()) return;

    bool bNewActive = !Direction.IsZero();

    if (bIsActive != bNewActive)
    {
        bIsActive = bNewActive;
        OnRep_IsActive();
    }
}

void AMovePillar::OnRep_IsActive()
{
    // 효과음 재생 등
}

void AMovePillar::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!MovingPlatformRoot) return;

    // 1. 활성화 (상승)
    if (bIsActive)
    {
        if (MoveDuration > 0.f) CurrentAlpha += DeltaTime / MoveDuration;
        else CurrentAlpha = 1.0f;

        CurrentAlpha = FMath::Clamp(CurrentAlpha, 0.0f, 1.0f);

        FVector NewLoc = FMath::Lerp(StartLocation, TargetLocation, CurrentAlpha);

        // 진동 (다 도착하면 멈춤)
        if (CurrentAlpha < 0.99f)
        {
            FVector Shake = FMath::VRand() * VibrationIntensity;
            NewLoc += Shake;
        }

        MovingPlatformRoot->SetRelativeLocation(NewLoc);
    }
    // 2. 비활성화 (하강/복귀)
    else
    {
        if (CurrentAlpha <= 0.0f)
        {
            if (!MovingPlatformRoot->GetRelativeLocation().Equals(StartLocation, 0.1f))
                MovingPlatformRoot->SetRelativeLocation(StartLocation);
            return;
        }

        float DecreaseRate = (ReturnDuration > 0.f) ? (DeltaTime / ReturnDuration) : 1.0f;
        CurrentAlpha -= DecreaseRate;
        CurrentAlpha = FMath::Clamp(CurrentAlpha, 0.0f, 1.0f);

        float CurveAlpha = CurrentAlpha;
        if (FallCurve) CurveAlpha = FallCurve->GetFloatValue(CurrentAlpha);

        FVector NewLoc = FMath::Lerp(StartLocation, TargetLocation, CurveAlpha);
        MovingPlatformRoot->SetRelativeLocation(NewLoc);
    }
}