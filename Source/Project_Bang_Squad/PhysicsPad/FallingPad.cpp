#include "FallingPad.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"

AFallingPad::AFallingPad()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    // RootComponent = MeshComp; 대신 아래 방식을 권장합니다.
    SetRootComponent(MeshComp);

    // 핵심: 메시가 물리적으로 '존재'함을 엔진에 알림
    MeshComp->SetMobility(EComponentMobility::Movable);
    MeshComp->SetCollisionProfileName(TEXT("BlockAllDynamic")); // 더 명확한 프로파일 사용
    MeshComp->SetSimulatePhysics(false);
    MeshComp->SetIsReplicated(true); // 컴포넌트 복제 활성화

    DetectionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DetectionBox"));
    DetectionBox->SetupAttachment(MeshComp); // RootComponent 대신 직접 MeshComp에 부착
    DetectionBox->SetCollisionProfileName(TEXT("Trigger"));
}

void AFallingPad::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority() && DetectionBox)
    {
        DetectionBox->OnComponentBeginOverlap.AddDynamic(this, &AFallingPad::OnBoxOverlap);
    }
}

void AFallingPad::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AFallingPad, bIsFalling);
}

void AFallingPad::OnBoxOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    UE_LOG(LogTemp, Warning, TEXT("Overlap Detected with: %s"), OtherActor ? *OtherActor->GetName() : TEXT("None"));

    if (HasAuthority() && !bIsFalling && OtherActor && OtherActor->IsA(ACharacter::StaticClass()))
    {
        UE_LOG(LogTemp, Warning, TEXT("Starting Fall Timer..."));
        DetectionBox->SetGenerateOverlapEvents(false);
        GetWorldTimerManager().SetTimer(FallTimerHandle, this, &AFallingPad::StartFalling, FallDelay, false);
    }
}

void AFallingPad::StartFalling()
{
    UE_LOG(LogTemp, Warning, TEXT("StartFalling Called on Server!"));
    if (!HasAuthority()) return;
    bIsFalling = true;
    ExecuteFall();
}

void AFallingPad::OnRep_bIsFalling()
{
    ExecuteFall();
}

void AFallingPad::ExecuteFall()
{
    if (MeshComp)
    {
        // 1. 트리거 박스가 물리 연산에 간섭하지 않도록 완전히 끕니다.
        if (DetectionBox)
        {
            DetectionBox->SetGenerateOverlapEvents(false);
            DetectionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }

        // 2. 가동성 및 콜리전 설정을 물리 가능 상태로 강제 전환
        MeshComp->SetMobility(EComponentMobility::Movable);
        MeshComp->SetCollisionProfileName(TEXT("BlockAllDynamic")); // 혹은 "PhysicsBody"
        MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        
        // 3. 물리 시뮬레이션 시작
        MeshComp->SetSimulatePhysics(true);
        
        // 4. 엔진에 물리 바디 갱신 요청 (핵심)
        MeshComp->RecreatePhysicsState();
        MeshComp->WakeRigidBody();

        UE_LOG(LogTemp, Warning, TEXT("ExecuteFall: Physics is now %s"), 
            MeshComp->IsSimulatingPhysics() ? TEXT("ENABLED") : TEXT("FAILED"));
    }
}