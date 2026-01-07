// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/Projectile/MageProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

 
AMageProjectile::AMageProjectile()
{
 // 1. 충돌체 설정
	SphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	SphereComp->InitSphereRadius(15.0f);

	// 투사ㅏ체 전용 충돌 프리셋 설정
	SphereComp->SetCollisionProfileName(TEXT("Projectile"));
	
	// 충돌시 OnHit 함수가 실행되도록 연결
	SphereComp->OnComponentHit.AddDynamic(this, &AMageProjectile::OnHit);
	RootComponent = SphereComp;
	
	// 2. 외형 메시 설정
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(RootComponent);
	// 메시 자체의 충돌을 끔
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	// 3. 발사체 이동 설정
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = SphereComp;
	ProjectileMovement->InitialSpeed = 3000.f; //발사 속도
	ProjectileMovement->MaxSpeed = 3000.f;
	ProjectileMovement->bRotationFollowsVelocity = true; // 이동 방향으로 회전
	ProjectileMovement->ProjectileGravityScale = 0.f; // 중력 영향 x
	
	// 4. 수명 설정
	InitialLifeSpan = 3.0f;
	
}


void AMageProjectile::BeginPlay()
{
	Super::BeginPlay();
	
}

void AMageProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	// 1. 유효한 액터에 부딪혔고, 나를 쏜 주인(Owner)이 아닐 때만 작동
	if (OtherActor && OtherActor != GetOwner())
	{
		// 2. 언리얼 기본 데미지 시스템 호출, 상대의 OnTakeAnyDamage를 발생시킴
		// 상대가 HealthComponent를 가지고 있다면 거기서 체력이 깎임
		UGameplayStatics::ApplyDamage(
			OtherActor,
			Damage,
			GetInstigatorController(),
			this,
			UDamageType::StaticClass());
		
		// 3. 부딪힌 곳에 이펙트를 남기거나 사운드를 재생할 위치 (추후 추가 가능)
		
		// 4. 부딪혔으므로 투사체 제거
		Destroy();
	}
}