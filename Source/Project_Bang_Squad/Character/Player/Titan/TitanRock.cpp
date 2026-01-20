#include "Project_Bang_Squad/Character/Player/Titan/TitanRock.h" // 경로가 맞는지 확인!
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h" // SphereOverlapActors용
#include "GameFramework/Character.h"    // LaunchCharacter용

ATitanRock::ATitanRock()
{
	PrimaryActorTick.bCanEverTick = false;

	// 1. 충돌체 생성
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	CollisionComp->InitSphereRadius(40.0f);
	CollisionComp->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	CollisionComp->SetNotifyRigidBodyCollision(true); // Hit 이벤트 필수

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
	SetLifeSpan(5.0f); // 5초 뒤 자동 삭제
}

void ATitanRock::InitializeRock(float InDamage, AActor* InOwner)
{
	Damage = InDamage;
	OwnerCharacter = InOwner;
	SetOwner(InOwner); // 주인 설정
}

void ATitanRock::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 나를 던진 주인과 바로 부딪히는 것 방지
	if (OtherActor == OwnerCharacter) return;

	// 무엇이든 부딪히면 폭발
	Explode();
}

void ATitanRock::Explode()
{
	FVector ExplosionLocation = GetActorLocation();

	// [로그] 폭발 확인용
	// UE_LOG(LogTemp, Warning, TEXT("TitanRock Exploded!"));

	// 1. 범위 내 액터 찾기
	TArray<AActor*> OverlappedActors;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));

	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(this); // 나 자신 제외

	UKismetSystemLibrary::SphereOverlapActors(
		GetWorld(),
		ExplosionLocation,
		ExplosionRadius,
		ObjectTypes,
		AActor::StaticClass(),
		IgnoreActors,
		OverlappedActors
	);

	// 2. 데미지 및 넉백 처리
	for (AActor* Victim : OverlappedActors)
	{
		if (!Victim || !Victim->IsValidLowLevel()) continue;

		// 아군 확인 ("Player" 태그)
		bool bIsAlly = Victim->ActorHasTag("Player");

		// 적군에게만 데미지
		if (!bIsAlly)
		{
			UGameplayStatics::ApplyDamage(
				Victim,
				Damage,
				OwnerCharacter ? OwnerCharacter->GetInstigatorController() : nullptr,
				OwnerCharacter,
				UDamageType::StaticClass()
			);
		}

		// 넉백 (모두 적용)
		FVector LaunchDir = (Victim->GetActorLocation() - ExplosionLocation).GetSafeNormal();
		LaunchDir.Z = 0.6f; // 위로 띄우기
		LaunchDir.Normalize();

		if (ACharacter* VictimChar = Cast<ACharacter>(Victim))
		{
			VictimChar->LaunchCharacter(LaunchDir * KnockbackForce, true, true);
		}
		else if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(Victim->GetRootComponent()))
		{
			if (RootComp->IsSimulatingPhysics())
			{
				RootComp->AddImpulse(LaunchDir * KnockbackForce * 100.0f);
			}
		}
	}

	// 3. 파괴
	Destroy();
}