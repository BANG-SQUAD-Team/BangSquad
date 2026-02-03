// [AQTEObject.cpp]
#include "AQTEObject.h"
#include "Project_Bang_Squad/Character/StageBoss/StageBossBase.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Project_Bang_Squad/Core/TrueDamageType.h"
#include "Project_Bang_Squad/Character/StageBoss/Stage1Boss.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

AQTEObject::AQTEObject()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true); // 움직임 동기화

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    RootComponent = MeshComp;
}

// [변수 복제 등록]
void AQTEObject::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // CurrentTapCount를 모든 클라이언트에게 복제
    DOREPLIFETIME(AQTEObject, CurrentTapCount);
}

void AQTEObject::BeginPlay()
{
    Super::BeginPlay();
}

void AQTEObject::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // [서버] 낙하 로직
    if (HasAuthority() && bIsFalling)
    {
        ElapsedTime += DeltaTime;
        float Alpha = FMath::Clamp(ElapsedTime / FallDuration, 0.0f, 1.0f);
        SetActorLocation(FMath::Lerp(StartLocation, TargetLocation, Alpha));
    }
}

// [낙하 초기화]
void AQTEObject::InitializeFalling(AActor* Target, float Duration)
{
    if (!HasAuthority() || !Target) return;

    TargetLocation = Target->GetActorLocation();
    TargetLocation.Z -= 100.0f; // 바닥 보정

    StartLocation = TargetLocation + FVector(0.0f, 0.0f, DropHeight);
    SetActorLocation(StartLocation);

    FallDuration = Duration;
    ElapsedTime = 0.0f;
    bIsFalling = true;
}

// [탭 등록]
void AQTEObject::RegisterTap()
{
    if (!HasAuthority()) return;

    CurrentTapCount++;
    UE_LOG(LogTemp, Warning, TEXT("QTE Tapped! Current Count: %d"), CurrentTapCount);

    if (CurrentTapCount >= TargetTapCount)
    {
        TriggerSuccess();
    }
}

// [성공 처리: 보스 처형]
void AQTEObject::TriggerSuccess()
{
    if (HasAuthority())
    {
        Multicast_PlayExplosion(true);

        // 1. [핵심] 월드에 있는 Stage1Boss를 직접 찾습니다. (GetOwner 사용 X)
        AActor* BossActor = UGameplayStatics::GetActorOfClass(GetWorld(), AStage1Boss::StaticClass());

        // 안전장치: 못 찾았으면 부모 클래스로 검색
        if (!BossActor)
        {
            BossActor = UGameplayStatics::GetActorOfClass(GetWorld(), AStageBossBase::StaticClass());
        }

        // 2. 보스 발견 시 처형 데미지 발송
        if (BossActor)
        {
            UE_LOG(LogTemp, Warning, TEXT(">>> TARGET FOUND: %s. Executing True Damage! <<<"), *BossActor->GetName());

            UGameplayStatics::ApplyDamage(
                BossActor,
                999999.0f,                      // 즉사 데미지
                nullptr,
                this,
                UTrueDamageType::StaticClass()  // [열쇠] 무적 관통 (TrueDamageType)
            );
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT(">>> ERROR: Boss Not Found! Damage Failed. <<<"));
        }

        SetLifeSpan(1.0f);
    }
}

// [실패 처리: 이펙트만]
// (플레이어 전멸은 보스쪽 HandleQTEResult에서 처리한다고 하셨으므로 여기선 이펙트만 냄)
void AQTEObject::TriggerFailure()
{
    if (HasAuthority())
    {
        Multicast_PlayExplosion(false);
        SetLifeSpan(0.5f);
    }
}

void AQTEObject::OnRep_CurrentTapCount()
{
    // 비주얼 갱신 필요 시 구현
}

void AQTEObject::Multicast_PlayExplosion_Implementation(bool bIsSuccess)
{
    if (ExplosionFX)
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionFX, GetActorLocation(), GetActorRotation(), FVector(3.0f));
    }
}