#include "BossSpikeTrap.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/OverlapResult.h"
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"
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
    // [권한 분리] 서버(Authority)에서만 로직을 수행합니다.
    // 이유: 데미지 판정, 충돌 감지 등 게임플레이에 중요한 로직은 서버가 주도권을 가져야 해킹을 방지하고 동기화를 맞출 수 있습니다.
    if (!HasAuthority()) return;

    // 1. 비주얼 실행 (NetMulticast)
    // 이유: 서버가 명령을 내리면 모든 클라이언트(나 자신 포함)에서 가시가 솟아오르는 연출을 재생합니다.
    Multicast_ActivateVisuals();

    // 2. 범위 판정 (Sphere Overlap)
    TArray<FOverlapResult> OverlapResults;
    FVector Center = GetActorLocation();

    // 트랩의 반경만큼 구체(Sphere) 모양의 충돌 범위를 생성합니다.
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(TrapRadius);

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this); // 나 자신(트랩)은 충돌 검사에서 제외
    if (GetInstigator())
    {
        QueryParams.AddIgnoredActor(GetInstigator()); // 트랩을 설치한 주체(보스 등)도 데미지를 입지 않도록 제외
    }

    // Pawn 채널(캐릭터들)에 해당하는 오브젝트들만 검출합니다.
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
        // [중복 방지] 한 프레임에 같은 액터가 여러 번 감지되는 것을 막기 위한 Set입니다.
        // 이유: OverlapMulti는 물리 엔진 특성상 컴포넌트가 여러 개면 한 액터를 여러 번 리턴할 수도 있습니다.
        TSet<AActor*> DamagedActors;

        for (const FOverlapResult& Result : OverlapResults)
        {
            AActor* HitActor = Result.GetActor();

            // [유효성 검사]
            // 1. 액터가 유효하지 않거나(삭제됨 등)
            // 2. 이미 이번 로직에서 처리를 완료한 액터라면 건너뜁니다.
            if (!IsValid(HitActor) || DamagedActors.Contains(HitActor)) continue;

            // [추가된 로직: 사망자 필터링]
            // 이유: 이미 죽어서 시체 상태인 플레이어를 다시 띄우거나 데미지를 주는 버그를 방지합니다.
            // HealthComponent를 찾아서 IsDead() 상태인지 확인합니다.
            UHealthComponent* HealthComp = HitActor->FindComponentByClass<UHealthComponent>();
            if (HealthComp && HealthComp->IsDead())
            {
                // 죽은 대상은 타겟팅 목록에서 제외하고 다음 대상으로 넘어갑니다.
                continue;
            }

            // [중복 방지 등록] 이 액터는 이제 처리되었음을 기록합니다.
            DamagedActors.Add(HitActor);

            // 3. 데미지 처리
            // 이유: 언리얼 내장 데미지 시스템을 사용하여 HP 감소, 피격 이벤트 등을 유발합니다.
            UGameplayStatics::ApplyDamage(
                HitActor,
                TrapDamage,
                GetInstigatorController(),
                this,
                UDamageType::StaticClass()
            );

            // 디버깅을 위한 로그 (개발 빌드에서만 확인용)
            UE_LOG(LogTemp, Warning, TEXT("[Trap] Dealt %.1f Damage to %s"), TrapDamage, *HitActor->GetName());

            // 4. 에어본(Airborne) 처리
            // 이유: 플레이어를 공중으로 띄워 게임플레이에 긴장감을 주고, 피격감을 강화합니다.
            ACharacter* HitCharacter = Cast<ACharacter>(HitActor);
            if (HitCharacter)
            {
                // Z축(위쪽)으로 솟구치는 힘을 만듭니다.
                FVector LaunchVelocity(0.f, 0.f, AirborneStrength);

                // XYOverride, ZOverride를 true로 설정하여 현재 움직임을 무시하고 강제로 띄웁니다.
                HitCharacter->LaunchCharacter(LaunchVelocity, true, true);
            }
        }
    }

    // 트랩의 역할이 끝났으므로 3초 뒤에 메모리에서 제거합니다.
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