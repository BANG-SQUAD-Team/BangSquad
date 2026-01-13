#include "Project_Bang_Squad/Character/Pillar.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h" // [필수] 리플리케이션 헤더

APillar::APillar()
{
    PrimaryActorTick.bCanEverTick = false;

    // [멀티플레이 필수 설정]
    bReplicates = true;             // 이 액터는 네트워크 통신을 수행
    SetReplicateMovement(true);     // 이 액터가 움직이면 위치를 동기화

    PillarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PillarMesh"));
    RootComponent = PillarMesh;

    PillarMesh->SetSimulatePhysics(false); 
    PillarMesh->SetNotifyRigidBodyCollision(true);
    
    // 멀티플레이에서 끊김 없이 움직이게 하려면 Collision 설정이 중요함
    PillarMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}

// 동기화 규칙 설정 
void APillar::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // bIsFallen 변수를 모든 클라이언트에게 동기화한다.
    DOREPLIFETIME(APillar, bIsFallen);
}

void APillar::BeginPlay()
{
    Super::BeginPlay();
    
    // Hit 이벤트는 서버에서만 처리해도 충분함 (데미지 판정 등)
    if (HasAuthority())
    {
        PillarMesh->OnComponentHit.AddDynamic(this, &APillar::OnPillarHit);
    }
}

// 서버가 이 함수를 실행하면 -> bIsFallen이 true가 되고 -> 클라이언트들은 OnRep_IsFallen이 실행됨
void APillar::TriggerFall()
{
    // 권한(서버)이 없으면 실행하지 마라 (방어 코드)
    if (!HasAuthority()) return;
    if (bIsFallen || !PillarMesh) return;

    // 1. 상태 변경 (이 순간 모든 클라이언트에게 신호가 감)
    bIsFallen = true;

    // 2. 서버에서도 물리 효과를 켜고 힘을 줘야 함 (OnRep은 서버에서 자동호출 안 되므로 수동 호출)
    OnRep_IsFallen();

    // 3. 물리적인 힘(Impulse)은 '서버'에서만 가하면 됩니다. (ReplicateMovement가 위치를 전파함)
    FVector LaunchDir = FallDirection.GetSafeNormal();
    FVector TorqueAxis = FVector::CrossProduct(FVector::UpVector, LaunchDir);
    
    PillarMesh->SetPhysicsAngularVelocityInDegrees(TorqueAxis * 100.f);
    FVector ApplyLocation = GetActorLocation() + FVector(0, 0, 400.f);
    PillarMesh->AddImpulseAtLocation(LaunchDir * FallForce, ApplyLocation);

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, TEXT("서버: 기둥 넘어짐 처리 완료"));
}

// 클라이언트들이 "어? 기둥 넘어졌네?" 하고 실행하는 함수
void APillar::OnRep_IsFallen()
{
    if (!PillarMesh) return;

    // 클라이언트들도 물리를 켜줘야 자연스럽게 보일 수 있음
    // (단, 위치 동기화는 서버가 ReplicateMovement로 덮어씌움)
    PillarMesh->SetSimulatePhysics(true);
    PillarMesh->WakeAllRigidBodies();

    // 캐릭터랑 안 부딪히게 변경
    PillarMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void APillar::OnPillarHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    // 데미지 처리는 오직 서버(Authority)에서만!
    if (HasAuthority() && bIsFallen && OtherComp->GetCollisionObjectType() == ECC_WorldStatic)
    {
       UGameplayStatics::ApplyRadialDamage(GetWorld(), 50.f, GetActorLocation(), 500.f, nullptr, {}, this);
       SetLifeSpan(3.0f); // 3초 뒤 삭제도 서버가 결정하면 다 같이 사라짐
    }
}