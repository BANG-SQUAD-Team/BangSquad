// Source/Project_Bang_Squad/Character/MonsterBase/WebProjectile.cpp

#include "WebProjectile.h"
// [헤더 경로 수정 반영]
#include "Project_Bang_Squad/PhysicsPad/WebPad.h"
#include "Project_Bang_Squad/Character/StageBoss/Cocoon.h"

#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

AWebProjectile::AWebProjectile()
{
	bReplicates = true;
	SetReplicateMovement(true);

	// 1. 충돌체 설정
	SphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	SphereComp->InitSphereRadius(20.0f);
	SphereComp->SetCollisionProfileName(TEXT("Custom"));
	SphereComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SphereComp->SetCollisionResponseToAllChannels(ECR_Ignore); // 1. 일단 모든 것을 무시

	SphereComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);        // 2. 플레이어(Pawn)와는 충돌해서 고치 생성
	SphereComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block); // 3. 바닥/기둥(WorldStatic)과 충돌해서 장판 생성

	// ※ 플레이어의 검이나 투사체(보통 WorldDynamic)는 Ignore 상태이므로 허공에서 부딪히지 않고 통과합니다!
	// =========================================================================

	SphereComp->OnComponentHit.AddDynamic(this, &AWebProjectile::OnHit);
	RootComponent = SphereComp;

	// 2. 메쉬 설정
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(SphereComp);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 3. 이동 컴포넌트 설정
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->InitialSpeed = 1500.0f; // 보스 코드에서 덮어씌우더라도 기본값 설정
	ProjectileMovement->MaxSpeed = 1500.0f;
	ProjectileMovement->ProjectileGravityScale = 1.0f; // 직선 발사 0.0f ,포물선 1.0f)
}

void AWebProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		SetLifeSpan(5.0f); // 못 맞춰도 5초 뒤 삭제
	}
}

void AWebProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 서버에서만 판정 & 나 자신(보스)이나 투사체 끼리 충돌 무시
	if (!HasAuthority()) return;
	if (!OtherActor || OtherActor == GetOwner() || OtherActor == this) return;

	// -------------------------------------------------------------------------
	// 1. 플레이어 직격 -> 코치(Cocoon) 생성하여 가두기
	// -------------------------------------------------------------------------
	ACharacter* HitCharacter = Cast<ACharacter>(OtherActor);

	// 플레이어이고, 적(Enemy) 태그가 없는 경우 (아군 오폭 방지)
	if (HitCharacter && !HitCharacter->ActorHasTag("Enemy"))
	{
		// CocoonClass는 헤더(WebProjectile.h)에 선언되어 있어야 합니다.
		if (CocoonClass)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			// 플레이어 위치에 고치 스폰
			ACocoon* NewCocoon = GetWorld()->SpawnActor<ACocoon>(CocoonClass, HitCharacter->GetActorLocation(), FRotator::ZeroRotator, Params);
			if (NewCocoon)
			{
				// [핵심] 고치에게 "이 플레이어를 가둬라" 명령
				NewCocoon->InitializeCocoon(HitCharacter);
			}
		}

		Destroy(); // 투사체 삭제
		return;
	}

	// -------------------------------------------------------------------------
	// 2. 바닥/벽 충돌 -> 거미줄 장판(WebPad) 생성
	// -------------------------------------------------------------------------
	// WorldStatic(지형)이거나 WorldDynamic(물체) 등에 닿았을 때
	if (OtherComp->GetCollisionObjectType() == ECC_WorldStatic || OtherComp->GetCollisionObjectType() == ECC_WorldDynamic)
	{
		// WebPadClass(또는 TrapClass)는 헤더에 선언되어 있어야 합니다.
		if (WebPadClass) // 기존 TrapClass 변수명을 WebPadClass로 쓰신다면 맞춰주세요
		{
			// 위치: 충돌 지점에서 법선 벡터 방향으로 살짝 위 (파묻힘 방지)
			FVector SpawnLoc = Hit.Location + (Hit.Normal * 5.0f);

			// 회전: 바닥 경사에 맞춰 눕히기
			FRotator SpawnRot = Hit.Normal.Rotation();
			SpawnRot.Pitch -= 90.0f; // 메쉬의 축에 따라 -90 혹은 +90 조정 필요

			// 바닥이 평평하면(Z가 위를 향하면) 그냥 정방향으로 깔끔하게
			// if (Hit.Normal.Z > 0.9f) SpawnRot = FRotator::ZeroRotator; (취향에 따라 선택)

			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			AWebPad* NewPad = GetWorld()->SpawnActor<AWebPad>(WebPadClass, SpawnLoc, SpawnRot, Params);
			if (NewPad)
			{
				// 생성된 장판 추가 설정 (필요하다면)
				// NewPad->SetLifeSpan(15.0f); // 영구 지속이 아니라면 시간 설정
			}
		}

		Destroy(); // 투사체 삭제
	}
}