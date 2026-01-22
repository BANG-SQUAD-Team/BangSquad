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

void ABossSpikeTrap::TriggerTrapLogic()
{
    // [권한 분리]: 중요 로직은 서버에서만
    if (!HasAuthority()) return;

    // 1. 비주얼 동기화 (모든 클라에 가시 올라오는 연출 명령)
    Multicast_ActivateVisuals();

    // 2. 범위 판정 (Overlap Sphere)
    TArray<FOverlapResult> OverlapResults;
    FVector Center = GetActorLocation();
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(TrapRadius);

    // Pawn(캐릭터) 타입만 검사하도록 설정
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this); // 자기 자신 제외
    if (GetInstigator()) QueryParams.AddIgnoredActor(GetInstigator()); // 보스(시전자) 제외

    // 월드에 겹친 액터 검사
    bool bHit = GetWorld()->OverlapMultiByObjectType(
        OverlapResults,
        Center,
        FQuat::Identity,
        FCollisionObjectQueryParams(ECollisionChannel::ECC_Pawn), // 폰만 검사
        SphereShape,
        QueryParams
    );

    if (bHit)
    {
        for (const FOverlapResult& Result : OverlapResults)
        {
            AActor* HitActor = Result.GetActor();
            if (!IsValid(HitActor)) continue;

            // 3. 데미지 처리
            UGameplayStatics::ApplyDamage(HitActor, TrapDamage, GetInstigatorController(), this, UDamageType::StaticClass());

            // 4. 에어본 처리 (LaunchCharacter)
            ACharacter* HitCharacter = Cast<ACharacter>(HitActor);
            if (HitCharacter)
            {
                // Z축으로 강하게 띄움. XYOverride, ZOverride를 true로 하여 기존 관성 무시
                FVector LaunchVelocity(0.f, 0.f, AirborneStrength);
                HitCharacter->LaunchCharacter(LaunchVelocity, true, true);
            }
        }
    }

    // 5. 액터 수명 종료 (연출 끝난 뒤 3초 후)
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