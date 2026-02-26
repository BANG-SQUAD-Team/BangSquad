#include "ArenaProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"


AArenaProjectile::AArenaProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	
	// 1. 충돌체 설정
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SpherComp"));
    CollisionComp->InitSphereRadius(15.0f);
	CollisionComp->BodyInstance.SetCollisionProfileName("Projectile"); // 투사체 판정
	CollisionComp->OnComponentHit.AddDynamic(this, &AArenaProjectile::OnHit);
	RootComponent = CollisionComp;
	
	// 2. 외형 메쉬 설정
	MeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(RootComponent);
	MeshComp->SetCollisionProfileName("NoCollision"); 
	
	// 3. 발사체 움직임 설정
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileComp"));
	ProjectileMovement->UpdatedComponent = CollisionComp;
	ProjectileMovement->InitialSpeed = 2000.0f;
	ProjectileMovement->MaxSpeed = 2000.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = true; // 땅에 닿으면 통통 튀도록
	ProjectileMovement->Bounciness = 0.3f;
	
	
}


void AArenaProjectile::BeginPlay()
{
	Super::BeginPlay();
	// 3초 뒤에 허공에서 자동 소멸 (메모리 관리)
	SetLifeSpan(3.0f);
}

void AArenaProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 서버에서만 넉백과 데미지 계산!
	if (!HasAuthority()) return;

	// 나 자신이나 나를 던진 사람(Owner)은 무시
	if (OtherActor && OtherActor != this && OtherActor != GetOwner())
	{
		// 1. 맞은 대상이 캐릭터인지 확인
		if (ACharacter* HitChar = Cast<ACharacter>(OtherActor))
		{
			// 2. 데미지 적용
			UGameplayStatics::ApplyDamage(HitChar, Damage, GetInstigatorController(), this, UDamageType::StaticClass());

			// 3. 넉백 방향 계산 (투사체에서 캐릭터로 향하는 방향)
			FVector KnockbackDir = (HitChar->GetActorLocation() - GetActorLocation()).GetSafeNormal();
			KnockbackDir.Z = 0.0f; // 일단 평면 방향만 추출
			KnockbackDir.Normalize();

			// 4. 넉백 파워와 띄우는 힘 합치기
			FVector LaunchForce = (KnockbackDir * KnockbackPower) + FVector(0.f, 0.f, UpwardForce);

			// 5. 발사!! (진행 중이던 움직임 무시하고 튕겨냄)
			HitChar->LaunchCharacter(LaunchForce, true, true);
            
			// 6. 플레이어를 맞췄으니 투사체 파괴
			Destroy();
		}
	}
}