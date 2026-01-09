#include "Project_Bang_Squad/Projectile/MageProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Particles/ParticleSystemComponent.h" // [필수] 파티클 헤더

AMageProjectile::AMageProjectile()
{
    // [멀티플레이 필수] 액터 및 움직임 동기화
    bReplicates = true;
    SetReplicateMovement(true);

    // 1. 구체 충돌체 설정
    SphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
    SphereComp->InitSphereRadius(15.0f);
    
    // [중요] 충돌 프로필 설정
    // Projectile 프로필이 있다면 그걸 쓰는 게 가장 좋음: SphereComp->SetCollisionProfileName(TEXT("Projectile"));
    // 없다면 아래처럼 수동 설정 (Pawn과는 겹치고, WorldStatic에는 막힘 등)
    SphereComp->SetCollisionProfileName(TEXT("OverlapAllDynamic")); 
    SphereComp->SetGenerateOverlapEvents(true);
    
    RootComponent = SphereComp;

    // 2. 외형 메시 설정 (필요 없다면 비워둬도 됨)
    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(RootComponent);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);
    MeshComp->SetGenerateOverlapEvents(false);

    // [추가] 3. 파티클 컴포넌트 (마법 이펙트)
    ParticleComp = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("ParticleComp"));
    ParticleComp->SetupAttachment(RootComponent);

    // 4. 발사체 이동 설정
    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->UpdatedComponent = SphereComp;
    ProjectileMovement->InitialSpeed = 2000.f; // 속도 조절
    ProjectileMovement->MaxSpeed = 2000.f;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->ProjectileGravityScale = 0.f; // 중력 0 (직선 비행)

    // 충돌 함수 연결
    SphereComp->OnComponentBeginOverlap.AddDynamic(this, &AMageProjectile::OnOverlap);
    
    InitialLifeSpan = 3.0f; // 3초 뒤 자동 삭제
}

void AMageProjectile::BeginPlay()
{
    Super::BeginPlay();

    // =========================================================
    // [핵심 Fix] 생성되자마자 주인(나)은 물리적으로 무시해버림
    // =========================================================
    if (GetOwner())
    {
        SphereComp->IgnoreActorWhenMoving(GetOwner(), true);
        
        // 안전장치: 캡슐 컴포넌트까지 명시적으로 무시
        if (UPrimitiveComponent* OwnerRoot = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()))
        {
            SphereComp->IgnoreComponentWhenMoving(OwnerRoot, true);
        }
    }
}

void AMageProjectile::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
                                UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // 예외 처리
    if (!OtherActor || OtherActor == this) return;

    // BeginPlay에서 무시 설정을 했지만, 한 번 더 체크 (안전장치)
    if (GetOwner() && OtherActor == GetOwner()) return;

    // [중요] 데미지 판정과 파괴는 오직 '서버'에서만
    if (HasAuthority())
    {
        // 1. 물리 정지 (위치 동기화 중단)
        if (ProjectileMovement)
        {
            ProjectileMovement->StopMovementImmediately();
        }

        // 2. 더 이상 충돌 안 나게 끄기
        SphereComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        
        // 3. 모습 숨기기 (Destroy 되기 전 짧은 순간을 위해)
        SetActorHiddenInGame(true); 

        // 4. 데미지 전달
        UGameplayStatics::ApplyDamage(
            OtherActor, 
            Damage, 
            GetInstigatorController(), 
            this, 
            UDamageType::StaticClass()
        );

        // 5. 파괴 (서버 파괴 -> 클라 자동 파괴)
        Destroy();
    }
    else
    {
        // (선택 사항) 클라이언트라면 여기서 펑! 하는 이펙트나 소리를 재생할 수도 있음
        // 하지만 보통 Destroy되면 사라지므로 생략 가능
    }
}