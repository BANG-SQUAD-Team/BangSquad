

#include "Project_Bang_Squad/Character/Player/Paladin/PaladinSkill2Hammer.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"


APaladinSkill2Hammer::APaladinSkill2Hammer()
{
 	
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	
	// 1. 충돌체 설정
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	CollisionComp->SetSphereRadius(40.0f);
	CollisionComp->SetCollisionProfileName(TEXT("BlockAllDynamic")); 
	// 시전자가 밟고 지나가지 않게 Pawn은 무시하되 WorldStatic(땅) 은 블록
	CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	RootComponent = CollisionComp;
	
	// 2. 메쉬 설정
	HammerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HammerMesh"));
	HammerMesh->SetupAttachment(RootComponent);
	HammerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	// 3. 무브먼트 설정
	MovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("MovementComp"));
	MovementComp->InitialSpeed = 0.f; // 떨어지는 속도
	MovementComp->MaxSpeed = 3000.0f;
	MovementComp->bRotationFollowsVelocity = false;
	MovementComp->bShouldBounce = false; // 튕기지 않음
	MovementComp->ProjectileGravityScale = 0.0f; // 직선으로 꽂힘
}


void APaladinSkill2Hammer::BeginPlay()
{
	Super::BeginPlay();
	
	if (HasAuthority())
	{
		CollisionComp->OnComponentHit.AddDynamic(this, &APaladinSkill2Hammer::OnHammerHit);
	}
	
}

void APaladinSkill2Hammer::InitializeHammer(float InDamage, AActor* InCaster)
{
	Damage = InDamage;
	CasterActor = InCaster;
	
	// 시전자가 망치에 끼지 않도록 이동 무시 설정 (안전장치)
	if (CollisionComp && InCaster)
	{
		CollisionComp->IgnoreActorWhenMoving(InCaster, true);
	}

	// 회전은 무시하고, 이동 방향만 "지구 중심(아래)"으로 강제 설정
	if (MovementComp)
	{
		// X, Y는 0, Z는 -3500 (아래로 꽂힘)
		MovementComp->Velocity = FVector(0.0f, 0.0f, -3500.0f);
		MovementComp->UpdateComponentVelocity();
	}
}

void APaladinSkill2Hammer::OnHammerHit(UPrimitiveComponent* HitComp, AActor* OtherActor, 
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 땅이나 물체에 닿았을시 폭발
	Explode();
	
	Multicast_PlayImpactVFX(Hit.ImpactPoint);
	
	SetLifeSpan(0.1f);
}

void APaladinSkill2Hammer::Multicast_PlayImpactVFX_Implementation(FVector Location)
{
	// 여기서 이펙트를 켜야 클라이언트들도 볼 수 있음
	if (ImpactVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			 GetWorld(), 
			 ImpactVFX, 
			 Location,
			 FRotator::ZeroRotator,
			 ImpactVFXScale, // 아까 만든 크기 변수
			 true, true, ENCPoolMethod::None, true
		);
	}
}

void APaladinSkill2Hammer::Explode()
{
	if (!HasAuthority()) return;
	
	FVector ImpactLoc = GetActorLocation();
	
	// 1. 범위 내 대상을 감지
	TArray<AActor*> OverlappedActors;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	
	UKismetSystemLibrary::SphereOverlapActors(
		GetWorld(),
		ImpactLoc,
		ExplosionRadius,
		ObjectTypes,
		ACharacter::StaticClass(),
		{},
		OverlappedActors
		);
	
	for (AActor* Victim : OverlappedActors)
	{
		if (!Victim) continue;
		
		ACharacter* VictimChar = Cast<ACharacter>(Victim);
		
		// ==================================
		// 데미지 (적군만 허용)
		// ==================================
		// Player 태그가 없으면 적으로 간주함
		if (VictimChar && !VictimChar->ActorHasTag("Player"))
		{
			UGameplayStatics::ApplyDamage(
				VictimChar,
				Damage,
				CasterActor ? CasterActor->GetInstigatorController() : nullptr,
				this,
				UDamageType::StaticClass()
				);
		}
		
		// ==================================
		// 에어본 & 넉백 (적군 + 아군 모드 적용)
		// ==================================
		if (VictimChar)
		{
			FVector LaunchForce = FVector::UpVector * 1800.0f;
			
			// 확실히 띄우기
			if (VictimChar->GetCharacterMovement())
			{
				VictimChar->GetCharacterMovement()->StopMovementImmediately();
				VictimChar->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			}
			
			VictimChar->LaunchCharacter(LaunchForce, true, true);
		}
	}
}

