#include "Boss1_Rampart.h"
#include "Net/UnrealNetwork.h"

ABoss1_Rampart::ABoss1_Rampart()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(false); // 수동 보간을 위해 false 설정

    // 루트 컴포넌트 생성
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;
}

void ABoss1_Rampart::BeginPlay()
{
    Super::BeginPlay();
    // 초기 위치 저장
    StartLocation = GetActorLocation();
    EndLocation = StartLocation - FVector(0.f, 0.f, TargetDepth);
}

void ABoss1_Rampart::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    UpdateMovement(DeltaTime);
}

void ABoss1_Rampart::UpdateMovement(float DeltaTime)
{
    if (RampartState == ERampartState::Idle) return;

    // 진행률(Progress) 업데이트
    float TargetAlpha = (RampartState == ERampartState::Sinking) ? 1.0f : 0.0f;
    // 목표값까지 일정한 속도로 보간
    CurrentProgress = FMath::FInterpConstantTo(CurrentProgress, TargetAlpha, DeltaTime, 1.0f / MoveDuration);

    // 1. 기본적인 상하 이동 위치 계산
    FVector NewLocation = FMath::Lerp(StartLocation, EndLocation, CurrentProgress);

    // 2. 진동 효과 (완전히 멈춘 상태가 아닐 때만 적용)
    if (CurrentProgress > 0.0f && CurrentProgress < 1.0f)
    {
        float Time = GetWorld()->GetTimeSeconds();
        float ShakeX = FMath::Sin(Time * ShakeFrequency) * ShakeIntensity;
        float ShakeY = FMath::Cos(Time * ShakeFrequency) * ShakeIntensity;
        NewLocation += FVector(ShakeX, ShakeY, 0.f);
    }

    SetActorLocation(NewLocation);

    // 이동 완료 체크
    if (FMath::IsNearlyEqual(CurrentProgress, TargetAlpha, 0.001f))
    {
        RampartState = ERampartState::Idle;
    }
}

void ABoss1_Rampart::Server_SetRampartActive_Implementation(bool bSink)
{
    RampartState = bSink ? ERampartState::Sinking : ERampartState::Rising;
}

void ABoss1_Rampart::OnRep_RampartState()
{
    // 클라이언트측 시각 효과/사운드가 필요할 경우 여기서 처리
}

void ABoss1_Rampart::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABoss1_Rampart, RampartState);
}