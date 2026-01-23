#include "BossSpikeTrap.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/OverlapResult.h"
#include "Net/UnrealNetwork.h"

ABossSpikeTrap::ABossSpikeTrap()
{
    PrimaryActorTick.bCanEverTick = true; // 타임라인용
    bReplicates = true;

    DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = DefaultSceneRoot;

    WarningDecalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WarningDecal"));
    WarningDecalMesh->SetupAttachment(RootComponent);
    WarningDecalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    SpikeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpikeMesh"));
    SpikeMesh->SetupAttachment(RootComponent);
    SpikeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 충돌 끔 (판정은 로직으로 처리)
    SpikeMesh->SetRelativeLocation(FVector(0.f, 0.f, -200.f)); // 바닥 아래 숨김
}

void ABossSpikeTrap::BeginPlay()
{
    Super::BeginPlay();

    // [서버]: 타이머 설정하여 공격 발동 예약
    if (HasAuthority())
    {
        FTimerHandle WarningTimer;
        GetWorld()->GetTimerManager().SetTimer(WarningTimer, this, &ABossSpikeTrap::TriggerTrapLogic, WarningDuration, false);
    }

    // [공통]: 타임라인 설정 (비주얼용)
    if (SpikeRiseCurve)
    {
        FOnTimelineFloat ProgressFunc;
        ProgressFunc.BindUFunction(this, FName("HandleSpikeProgress"));
        SpikeTimeline.AddInterpFloat(SpikeRiseCurve, ProgressFunc);
    }
}

void ABossSpikeTrap::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    SpikeTimeline.TickTimeline(DeltaTime);
}

// Source/Project_Bang_Squad/Character/StageBoss/BossSpikeTrap.cpp

void ABossSpikeTrap::TriggerTrapLogic()
{
    // [권한 분리]
    if (!HasAuthority()) return;

    // 1. 비주얼 실행
    Multicast_ActivateVisuals();

    // 2. 범위 판정
    TArray<FOverlapResult> OverlapResults;
    FVector Center = GetActorLocation();
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(TrapRadius);

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    if (GetInstigator()) QueryParams.AddIgnoredActor(GetInstigator());

    bool bHit = GetWorld()->OverlapMultiByObjectType(
        OverlapResults,
        Center,
        FQuat::Identity,
        FCollisionObjectQueryParams(ECollisionChannel::ECC_Pawn),
        SphereShape,
        QueryParams
    );

    if (bHit)
    {
        // [핵심] 중복 데미지 방지용 목록 (이번 프레임에 맞은 놈들 기록)
        TSet<AActor*> DamagedActors;

        for (const FOverlapResult& Result : OverlapResults)
        {
            AActor* HitActor = Result.GetActor();

            // 유효성 체크 및 "이미 맞았는지" 확인
            if (!IsValid(HitActor) || DamagedActors.Contains(HitActor)) continue;

            // 목록에 등록 (이제 넌 두 번 안 맞는다)
            DamagedActors.Add(HitActor);

            // 3. 데미지 처리 (이제 딱 한 번만 실행됨)
            UGameplayStatics::ApplyDamage(HitActor, TrapDamage, GetInstigatorController(), this, UDamageType::StaticClass());

            UE_LOG(LogTemp, Warning, TEXT("[Trap] Dealt %.1f Damage to %s"), TrapDamage, *HitActor->GetName());

            // 4. 에어본 처리
            ACharacter* HitCharacter = Cast<ACharacter>(HitActor);
            if (HitCharacter)
            {
                FVector LaunchVelocity(0.f, 0.f, AirborneStrength);
                HitCharacter->LaunchCharacter(LaunchVelocity, true, true);
            }
        }
    }

    SetLifeSpan(3.0f);
}


void ABossSpikeTrap::Multicast_ActivateVisuals_Implementation()
{
    // 가시 애니메이션 재생
    if (SpikeRiseCurve)
    {
        SpikeTimeline.PlayFromStart();
    }

    // 경고 장판 숨기기
    if (WarningDecalMesh)
    {
        WarningDecalMesh->SetVisibility(false);
    }
}

void ABossSpikeTrap::HandleSpikeProgress(float Value)
{
    // 비주얼: 가시가 땅에서 솟아오름
    float NewZ = FMath::Lerp(-200.f, 0.f, Value);
    SpikeMesh->SetRelativeLocation(FVector(0.f, 0.f, NewZ));
}