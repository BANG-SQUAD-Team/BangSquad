#include "Project_Bang_Squad/Character/Player/Mage/Stage2MagicBall.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"

AStage2MagicBall::AStage2MagicBall()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);

  
    
    // 1. 메쉬 설정
    BallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BallMesh"));
    RootComponent = BallMesh;

    // 물리 켜기
    BallMesh->SetSimulatePhysics(true);
    BallMesh->SetEnableGravity(true); // 중력 0으로 하셨다 했는데, 절벽 떨어지려면 켜야 함 (일단 취향껏)
    
    // [중요] 마찰력 0으로 만들기 (이러면 한번 밀면 영원히 굴러감)
    BallMesh->SetLinearDamping(0.0f); 
    BallMesh->SetAngularDamping(0.0f);

    // 콜리전: 바닥(WorldStatic)은 막아야 함!
    BallMesh->SetCollisionProfileName(TEXT("Custom"));
    BallMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    BallMesh->SetCollisionResponseToAllChannels(ECR_Block);
    BallMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap); // 캐릭터만 통과
    BallMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    // 2. 트리거 설정
    DamageTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("DamageTrigger"));
    DamageTrigger->SetupAttachment(BallMesh);
    DamageTrigger->SetBoxExtent(FVector(110.f, 110.f, 110.f)); 
    DamageTrigger->SetCollisionProfileName(TEXT("Trigger"));
    
    if (BallMesh)
    {
        
        BallMesh->SetLinearDamping(0.2f); 
        BallMesh->SetAngularDamping(0.1f);
    }
}

void AStage2MagicBall::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        DamageTrigger->OnComponentBeginOverlap.AddDynamic(this, &AStage2MagicBall::OnOverlapBegin);
        BallMesh->OnComponentBeginOverlap.AddDynamic(this, &AStage2MagicBall::OnOverlapBegin);
    }
}

void AStage2MagicBall::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

// =================================================================
// 인터페이스
// =================================================================

void AStage2MagicBall::SetMageHighlight(bool bActive)
{
    if (BallMesh) BallMesh->SetRenderCustomDepth(bActive);
}

void AStage2MagicBall::ProcessMageInput(FVector Direction)
{
    // 서버 권한 없거나 이미 구르고 있으면 무시
    if (!HasAuthority()) return;
    if (bIsRolling) return;

   
    if (Direction.Size() > 0.1f) 
    {
        StartRolling();
    }
}

// =================================================================
// 내부 로직
// =================================================================

void AStage2MagicBall::StartRolling()
{
    if (bIsRolling) return;
    bIsRolling = true;
    
    if (BallMesh)
    {
        BallMesh->WakeRigidBody();

        // 1. 초기 추진력 방향
        FVector PushDir = MoveDirection.GetSafeNormal();
        
        // 계단 아래로 깔아뭉개며 출발하도록 Z축 힘 추가
        PushDir.Z = -0.5f; 
        PushDir.Normalize();

        // 2. "Impulse" 대신 "Force"를 쓰거나, 아주 약한 Impulse를 준다
        //여기서는 그냥 bVelChange=true(질량 무시)로 하되, 속도를 확 줄여서 툭 미는 느낌만 준다
        
        
        
        float InitialPushSpeed = 300.0f; // 초기 속도는 느리게!
        
        BallMesh->SetPhysicsLinearVelocity(PushDir * InitialPushSpeed);
        
    }
}

void AStage2MagicBall::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, 
                                      UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
                                      bool bFromSweep, const FHitResult& SweepResult)
{
    if (!HasAuthority() || !bIsRolling) return;
    if (OtherActor == this) return;

    UGameplayStatics::ApplyDamage(OtherActor, DamageAmount, nullptr, this, UDamageType::StaticClass());

    ACharacter* HitChar = Cast<ACharacter>(OtherActor);
    if (HitChar)
    {
        FVector BallCenter = GetActorLocation();
        FVector TargetCenter = HitChar->GetActorLocation();
        FVector KnockbackDir = (TargetCenter - BallCenter).GetSafeNormal();
        
        KnockbackDir += FVector(0.f, 0.f, 0.6f);
        KnockbackDir.Normalize();
        
        HitChar->LaunchCharacter(KnockbackDir * 1500.0f, true, true);
    }
}