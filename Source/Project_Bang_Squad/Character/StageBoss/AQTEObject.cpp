// [AQTEObject.cpp]
#include "AQTEObject.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

AQTEObject::AQTEObject()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true); // 움직임도 서버가 통제하여 동기화

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    RootComponent = MeshComp;
}

// [변수 복제 등록]
void AQTEObject::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // CurrentTapCount를 모든 클라이언트에게 복제함
    DOREPLIFETIME(AQTEObject, CurrentTapCount);
}

void AQTEObject::BeginPlay()
{
    Super::BeginPlay();
}

void AQTEObject::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // [서버] 위치 업데이트 로직 (서버가 움직이면 ReplicateMovement로 클라도 따라옴)
    if (HasAuthority() && bIsFalling)
    {
        ElapsedTime += DeltaTime;

        // 0.0(시작) ~ 1.0(도착) 비율 계산
        float Alpha = FMath::Clamp(ElapsedTime / FallDuration, 0.0f, 1.0f);

        // 선형 보간으로 위치 이동
        SetActorLocation(FMath::Lerp(StartLocation, TargetLocation, Alpha));

        // (옵션) 도착해도 타이머(GameMode)가 결과를 처리하므로 여기선 멈추기만 함
        if (Alpha >= 1.0f)
        {
            // bIsFalling = false; // 필요 시 주석 해제하여 멈춤 처리
        }
    }
}

// [핵심 로직 변경] 높이 설정 반영
void AQTEObject::InitializeFalling(AActor* Target, float Duration)
{
    // 서버 권한 확인
    if (!HasAuthority() || !Target) return;

    // 1. 도착점 계산 (타겟 위치)
    TargetLocation = Target->GetActorLocation();
    TargetLocation.Z -= 100.0f; // 바닥 보정 (발밑까지 찍도록)

    // 2. [수정됨] 시작점 계산 (도착점에서 DropHeight만큼 위로)
    StartLocation = TargetLocation + FVector(0.0f, 0.0f, DropHeight);

    // 3. 메쉬를 시작 위치로 강제 이동
    SetActorLocation(StartLocation);

    // 4. 낙하 설정
    FallDuration = Duration;
    ElapsedTime = 0.0f;
    bIsFalling = true;
}

void AQTEObject::RegisterTap()
{
    // [권한 분리]
    if (!HasAuthority()) return;

    CurrentTapCount++;

    UE_LOG(LogTemp, Warning, TEXT("QTE Tapped! Current Count: %d"), CurrentTapCount);

    if (CurrentTapCount >= TargetTapCount)
    {
        TriggerSuccess();
    }
}

void AQTEObject::TriggerSuccess()
{
    if (HasAuthority())
    {
        Multicast_PlayExplosion(true);
        SetLifeSpan(1.0f); // 1초 뒤 삭제
    }
}

void AQTEObject::TriggerFailure()
{
    if (HasAuthority())
    {
        Multicast_PlayExplosion(false);
        SetLifeSpan(0.5f); // 0.5초 뒤 삭제
    }
}

void AQTEObject::OnRep_CurrentTapCount()
{
    // [비주얼 동기화] UI 갱신 등 필요 시 구현
}

void AQTEObject::Multicast_PlayExplosion_Implementation(bool bIsSuccess)
{
    if (ExplosionFX)
    {
        // 폭발 이펙트 크기 3배로 키워서 재생
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionFX, GetActorLocation(), GetActorRotation(), FVector(3.0f));
    }
}