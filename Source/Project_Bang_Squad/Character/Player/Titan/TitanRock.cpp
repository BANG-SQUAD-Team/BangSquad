#include "Project_Bang_Squad/Character/Player/Titan/TitanRock.h" // 경로 주의
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h"
#include "Project_Bang_Squad/Character/StageBoss/StageBossBase.h"

ATitanRock::ATitanRock()
{
    PrimaryActorTick.bCanEverTick = false;

    // 1. 충돌체 생성
    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
    CollisionComp->InitSphereRadius(40.0f);
    CollisionComp->SetCollisionProfileName(TEXT("BlockAllDynamic")); // 물리 충돌 가능하게

    // [중요] 이 코드가 없으면 물리 이동 중인 물체는 OnHit 로그가 안 뜹니다!
    CollisionComp->SetNotifyRigidBodyCollision(true);

    RootComponent = CollisionComp;

    // 2. 바위 메시 생성
    RockMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RockMesh"));
    RockMesh->SetupAttachment(RootComponent);
    RockMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

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
    // [로그 1] 충돌 이벤트 자체가 발생하는지 확인
    // 화면에 빨간 글씨로 뜹니다.
    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, FString::Printf(TEXT("Rock Hit Actor: %s"), *OtherActor->GetName()));

    // UE_LOG로도 남깁니다 (하단 Output Log 탭 확인용)
    UE_LOG(LogTemp, Warning, TEXT("[TitanRock] Hit Actor: %s"), *OtherActor->GetName());

    // 1. 기본 체크
    if (!OtherActor || OtherActor == this || OtherActor == OwnerCharacter) return;

    // 2. 캐스팅 시도
    AEnemyCharacterBase* Enemy = Cast<AEnemyCharacterBase>(OtherActor);
    AStageBossBase* Boss = Cast<AStageBossBase>(OtherActor);

    // [로그 2] 캐스팅 결과 확인
    if (Enemy) UE_LOG(LogTemp, Warning, TEXT("[TitanRock] -> Cast Success: It is EnemyCharacterBase!"));
    if (Boss) UE_LOG(LogTemp, Warning, TEXT("[TitanRock] -> Cast Success: It is StageBossBase!"));

    if (Enemy || Boss)
    {
        // [로그 3] 데미지 함수 호출 직전 확인
        UE_LOG(LogTemp, Warning, TEXT("[TitanRock] ApplyDamage Called! Damage: %f"), Damage);

        UGameplayStatics::ApplyDamage(
            OtherActor,
            Damage,
            OwnerCharacter->GetInstigatorController(),
            OwnerCharacter,
            UDamageType::StaticClass()
        );

        Destroy();
    }
    else
    {
        // [로그 4] 적이 아닌 경우 (Cast 실패)
        UE_LOG(LogTemp, Warning, TEXT("[TitanRock] Hit something else (Wall/Floor?), Destroying."));
        Destroy();
    }
}