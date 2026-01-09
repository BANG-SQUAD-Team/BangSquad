#include "Project_Bang_Squad/Projectile/MageProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h" // [필수] 리플리케이션 헤더

AMageProjectile::AMageProjectile()
{
    // [멀티플레이 필수 1] 이 액터는 모든 클라이언트에게 복제됩니다.
    bReplicates = true;
    
    // [멀티플레이 필수 2] 움직임(위치/속도)도 동기화합니다.
    SetReplicateMovement(true);

    // 1. 구체 충돌체 설정
    SphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
    SphereComp->InitSphereRadius(15.0f);
    // OverlapAllDynamic은 통과하는 속성이므로, 필요하다면 Block을 섞거나 로직으로 처리
    SphereComp->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    SphereComp->SetGenerateOverlapEvents(true); 
    RootComponent = SphereComp;

    // 2. 외형 메시 설정
    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(RootComponent);
    
    // 메시 충돌 끄기 (최적화 및 버그 방지)
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);
    MeshComp->SetGenerateOverlapEvents(false);

    // 3. 발사체 이동 설정
    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->UpdatedComponent = SphereComp;
    ProjectileMovement->InitialSpeed = 3000.f;
    ProjectileMovement->MaxSpeed = 3000.f;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->ProjectileGravityScale = 0.f;

    // 충돌 함수 연결
    SphereComp->OnComponentBeginOverlap.AddDynamic(this, &AMageProjectile::OnOverlap);
    
    InitialLifeSpan = 3.0f;
}

void AMageProjectile::BeginPlay()
{
    Super::BeginPlay();
}

void AMageProjectile::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
                                UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // [중요] 데미지 판정과 액터 파괴는 권한(Authority)이 있는 '서버'에서만 수행합니다.
    // 클라이언트에서 Destroy()를 하면 싱크가 깨져서 유령 투사체가 남을 수 있습니다.
    if (HasAuthority())
    {
        // 나 자신, 그리고 나를 쏜 주인(Owner)과는 충돌하지 않음
        if (OtherActor && (OtherActor != this) && (OtherActor != GetOwner()))
        {
            // 1. 물리 연산 중단 (서버에서 멈추면 위치 동기화로 인해 클라이언트도 멈춤)
            if (ProjectileMovement)
            {
                ProjectileMovement->StopMovementImmediately();
            }

            // 2. 충돌 끄기 및 숨기기 (서버에서 끄면 클라이언트도 적용됨)
            SphereComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            // NetMulticast를 쓰지 않는 한, 서버에서 Visibility를 꺼도 클라이언트엔 바로 반영 안 될 수 있으나,
            // 바로 Destroy() 하므로 큰 문제는 없음.
            SetActorHiddenInGame(true); 

            // 3. 데미지 전달 (서버 -> BaseCharacter::TakeDamage -> HealthComponent)
            UGameplayStatics::ApplyDamage(OtherActor, Damage, GetInstigatorController(), this, UDamageType::StaticClass());

            // 4. 파괴 (서버에서 파괴되면 모든 클라이언트에서도 사라짐)
            Destroy();
        }
    }
}