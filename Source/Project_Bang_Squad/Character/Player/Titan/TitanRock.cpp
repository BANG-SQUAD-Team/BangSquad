#include "Project_Bang_Squad/Character/Player/Titan/TitanRock.h" // 경로 주의
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/ProjectileMovementComponent.h"

ATitanRock::ATitanRock()
{
    PrimaryActorTick.bCanEverTick = false;

    // 1. 충돌체 생성
    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
    CollisionComp->InitSphereRadius(40.0f);
    CollisionComp->SetCollisionProfileName(TEXT("BlockAllDynamic")); // 물리 충돌 가능하게
    RootComponent = CollisionComp;

    // 2. 바위 메시 생성
    RockMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RockMesh"));
    RockMesh->SetupAttachment(RootComponent);
    RockMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 충돌은 Sphere가 담당

    // 충돌 이벤트 바인딩
    CollisionComp->OnComponentHit.AddDynamic(this, &ATitanRock::OnHit);
}

void ATitanRock::BeginPlay()
{
    Super::BeginPlay();
    // 3초 뒤 자동 파괴 (너무 멀리 날아가는 것 방지)
    SetLifeSpan(5.0f);
}

void ATitanRock::InitializeRock(float InDamage, AActor* InOwner)
{
    Damage = InDamage;
    OwnerCharacter = InOwner;
}

void ATitanRock::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    if (!OtherActor || OtherActor == this || OtherActor == OwnerCharacter) return;

    // 적 태그 확인 (아군 피격 방지)
    if (OtherActor->ActorHasTag("Boss") || OtherActor->ActorHasTag("MidBoss") || OtherActor->ActorHasTag("Enemy"))
    {
        UGameplayStatics::ApplyDamage(OtherActor, Damage, OwnerCharacter->GetInstigatorController(), OwnerCharacter, UDamageType::StaticClass());

        // 타격 이펙트나 사운드는 여기에 추가
        Destroy(); // 맞으면 파괴
    }
    else
    {
        // 땅이나 벽에 맞으면 그냥 파괴
        Destroy();
    }
}