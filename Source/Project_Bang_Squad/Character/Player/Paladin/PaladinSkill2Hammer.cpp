

#include "Project_Bang_Squad/Character/Player/Paladin/PaladinSkill2Hammer.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Particles/ParticleSystemComponent.h"
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

// 맨 아래나 적당한 곳에 추가하세요

void APaladinSkill2Hammer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// 액터(망치)가 SetLifeSpan에 의해 죽을 때 실행됨
	if (LingeringVFXComp)
	{
		// 파티클 생성기 끄기 
		// 기존에 나온 연기나 불꽃은 자연스럽게 사라짐 
		LingeringVFXComp->DeactivateSystem();
		
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
	// 이미 터졌으면 중복 실행 방지
	if (CollisionComp->GetCollisionEnabled() == ECollisionEnabled::NoCollision) return;

	// 1. 폭발 로직 실행 (데미지, 넉백)
	Explode();
    
	// 2. 쾅! 하는 임팩트 이펙트 재생 (서버 -> 멀티캐스트)
	Multicast_PlayImpactVFX(Hit.ImpactPoint);
    
	// 3. 충돌 끄기 및 메쉬 숨기기 (망치는 사라진 것처럼 보임)
	if (HasAuthority())
	{
		CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 더 이상 충돌 안 함
        
		// 멀티캐스트로 메쉬 숨기고 잔류 이펙트 재생하라고 명령
		Multicast_StartLingeringEffect(Hit.ImpactPoint);
        
		// 4. 액터 삭제는 잔류 이펙트 시간만큼 기다렸다가 함
		SetLifeSpan(LingeringDuration + 0.5f); // 넉넉하게 0.5초 더 줌
	}
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

void APaladinSkill2Hammer::Multicast_StartLingeringEffect_Implementation(FVector Location)
{
	// 1. 망치 메쉬 안 보이게 하기
	if (HammerMesh)
	{
		HammerMesh->SetVisibility(false);
	}
    
	// 2. 움직임 멈추기
	if (MovementComp)
	{
		MovementComp->StopMovementImmediately();
	}

	// 3. [ParticleSystem] 잔류 이펙트(장판) 생성
	if (LingeringVFX)
	{
		LingeringVFXComp = UGameplayStatics::SpawnEmitterAtLocation(
		  GetWorld(),
		  LingeringVFX,
		  Location,
		  FRotator::ZeroRotator,
		  LingeringVFXScale, 
		  true,
		  EPSCPoolMethod::None,
		  true
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

