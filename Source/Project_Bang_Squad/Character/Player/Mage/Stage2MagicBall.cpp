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
    // [디버그 1] 메이지가 나를 건드리는지 확인
    // 화면에 빨간 글씨로 입력된 방향값(X, Y, Z)을 띄웁니다.
    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, FString::Printf(TEXT("🔥 INPUT RECEIVED: %s"), *Direction.ToString()));

    // 서버 권한 없거나 이미 구르고 있으면 무시
    if (!HasAuthority()) return;
    if (bIsRolling) return;

    // 🚨 [진단용 수정] 방향 따지지 말고, 입력이 조금이라도 들어오면 무조건 굴리기!
    // 원래 코드: if (Direction.Y > RequiredInputThreshold)
    // 수정 코드: 입력 크기가 0.1만 넘으면 출발 (방향 이슈 배제)
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

        // 2. [핵심] "Impulse" 대신 "Force"를 쓰거나, 아주 약한 Impulse를 줍니다.
        // bVelChange = false로 하면 "질량"을 반영해서 밀기 때문에
        // 질량이 5000kg이면 엄청난 힘이 필요합니다.
        // 여기서는 그냥 bVelChange=true(질량 무시)로 하되, 속도를 확 줄여서 '툭' 미는 느낌만 줍니다.
        
        // 기존 MoveSpeed가 1000이었다면 -> 300 ~ 500 정도로 줄이세요.
        // 처음에 '툭' 밀어주면, 나머지는 중력(Gravity Scale 2.0)이 알아서 가속시킵니다.
        
        float InitialPushSpeed = 300.0f; // 초기 속도는 느리게!
        
        BallMesh->SetPhysicsLinearVelocity(PushDir * InitialPushSpeed);
        
        // (선택) 굴러가는 회전력을 처음에 살짝 줍니다 (앞으로 구르게)
        // Y축이 오른쪽 진행방향이면, X축(앞)을 기준으로 회전
        // FVector TorqueDir = FVector(1.0f, 0.0f, 0.0f) * -1.0f; 
        // BallMesh->SetPhysicsAngularVelocityInDegrees(TorqueDir * 300.0f);
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