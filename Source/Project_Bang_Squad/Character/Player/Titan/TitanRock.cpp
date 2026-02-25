#include "Project_Bang_Squad/Character/Player/Titan/TitanRock.h" 
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h" 
#include "GameFramework/Character.h"   
#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h"
#include "Project_Bang_Squad/Character/Enemy/EnemyNormal.h"

ATitanRock::ATitanRock()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	CollisionComp->InitSphereRadius(40.0f);
	
	CollisionComp->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	CollisionComp->SetNotifyRigidBodyCollision(true); 

	RootComponent = CollisionComp;

	RockMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RockMesh"));
	RockMesh->SetupAttachment(RootComponent);
	RockMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CollisionComp->OnComponentHit.AddDynamic(this, &ATitanRock::OnHit);
}

void ATitanRock::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(5.0f); 
}

void ATitanRock::InitializeRock(float InDamage, AActor* InOwner)
{
	Damage = InDamage;
	OwnerCharacter = InOwner;
	SetOwner(InOwner); 
}

void ATitanRock::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (OtherActor == OwnerCharacter) return;

	Explode();
}

void ATitanRock::Explode()
{
	FVector ExplosionLocation = GetActorLocation();

	TArray<AActor*> OverlappedActors;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));

	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(this); // �� �ڽ� ����

	UKismetSystemLibrary::SphereOverlapActors(
		GetWorld(),
		ExplosionLocation,
		ExplosionRadius,
		ObjectTypes,
		AActor::StaticClass(),
		IgnoreActors,
		OverlappedActors
	);

	for (AActor* Victim : OverlappedActors)
	{
		if (!Victim || !Victim->IsValidLowLevel()) continue;

		// 아군 확인 ("Player" 태그)
		bool bIsAlly = Victim->ActorHasTag("Player");

		// 아군이 아니면 데미지 적용
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

		// [핵심 로직] 에너미 베이스를 상속받았는데, 노말 몹이 아니라고? -> 보스급 몬스터!
		bool bIsBoss = Victim->IsA(AEnemyCharacterBase::StaticClass()) && !Victim->IsA(AEnemyNormal::StaticClass());

		// 넉백 (보스가 아닐 때만 띄워버림!)
		if (!bIsBoss)
		{
			FVector LaunchDir = (Victim->GetActorLocation() - ExplosionLocation).GetSafeNormal();
			LaunchDir.Z = 0.6f; // 위로 띄움
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
	}

	Destroy();
}