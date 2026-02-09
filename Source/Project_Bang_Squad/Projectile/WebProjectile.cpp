#include "WebProjectile.h"
// [수정] WebPad 헤더 포함 (경로 주의: PhysicsPad 폴더에 있음)
#include "Project_Bang_Squad/PhysicsPad/WebPad.h" 
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

AWebProjectile::AWebProjectile()
{
	bReplicates = true;
	SetReplicateMovement(true);

	SphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	SphereComp->InitSphereRadius(20.0f);
	SphereComp->SetCollisionProfileName(TEXT("Projectile"));
	RootComponent = SphereComp;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(SphereComp);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->InitialSpeed = 1200.0f;
	ProjectileMovement->MaxSpeed = 1200.0f;
	ProjectileMovement->ProjectileGravityScale = 1.0f;
}

void AWebProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		SphereComp->OnComponentHit.AddDynamic(this, &AWebProjectile::OnHit);
		SetLifeSpan(5.0f);
	}
}

void AWebProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority()) return;
	if (OtherActor == GetOwner()) return;

	ACharacter* HitCharacter = Cast<ACharacter>(OtherActor);

	// 1. 플레이어 직격: 데미지 + 속박
	if (HitCharacter)
	{
		UGameplayStatics::ApplyDamage(HitCharacter, 20.0f, GetInstigatorController(), this, UDamageType::StaticClass());

		// [추가 가능] 여기서 AWebPad의 디버프 함수를 직접 호출하거나
		// 별도의 속박(Rooting) 상태 이상을 걸어줄 수 있습니다.

		Destroy();
	}
	// 2. 바닥/벽 충돌: WebPad(거미줄 장판) 생성
	else
	{
		if (WebPadClass)
		{
			FVector SpawnLoc = Hit.Location + (Hit.Normal * 5.0f);
			FRotator SpawnRot = Hit.Normal.Rotation();

			// 바닥이면 평평하게
			if (Hit.Normal.Z > 0.8f) SpawnRot = FRotator::ZeroRotator;

			AWebPad* NewPad = GetWorld()->SpawnActor<AWebPad>(WebPadClass, SpawnLoc, SpawnRot);
			if (NewPad)
			{
				// [중요] 기존 AWebPad에는 자동 파괴 로직이 없으므로 여기서 수명 설정
				NewPad->SetLifeSpan(15.0f);
			}
		}
		Destroy();
	}
}