#include "ZRotationActor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/EngineTypes.h" // ECB_No 및 MOVE_Falling 정의 포함

AZRotationActor::AZRotationActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// 1. 컴포넌트 생성 및 루트 설정
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;

	// 2. 네트워크 복제 설정
	bReplicates = true;
	SetReplicateMovement(true);

	if (MeshComponent)
	{
		MeshComponent->SetNotifyRigidBodyCollision(true);
		MeshComponent->SetCollisionProfileName(TEXT("BlockAll"));

		// ECSU_No 대신 ECB_No를 사용하여 캐릭터가 회전력을 상속받지 못하게 함
		MeshComponent->CanCharacterStepUpOn = ECB_No;

		MeshComponent->BodyInstance.bUseCCD = true;

		MeshComponent->SetCanEverAffectNavigation(false);
	}
}

void AZRotationActor::BeginPlay()
{
	Super::BeginPlay();

	if (MeshComponent)
	{
		MeshComponent->OnComponentHit.AddDynamic(this, &AZRotationActor::OnMeshHit);
	}
}

void AZRotationActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 서버에서만 회전 계산 (SetReplicateMovement가 위치/회전 복제를 처리함)
	if (HasAuthority())
	{
		float DirectionMultiplier = bClockwise ? 1.0f : -1.0f;
		float DeltaDegrees = RotationSpeed * DirectionMultiplier * DeltaTime;
		FRotator DeltaRotation = FRotator(0.0f, DeltaDegrees, 0.0f);

		// 액터 자체의 Z축(Local Z)을 기준으로 회전하도록 설정
		AddActorLocalRotation(FRotator(0.0f, DeltaDegrees, 0.0f));

		FHitResult SweepHit;
		AddActorLocalRotation(DeltaRotation, true, &SweepHit, ETeleportType::None);

		// 혹시라도 뚫고 들어온 놈이 있다면 즉시 추방
		PreventClipping();
	}
}

void AZRotationActor::OnMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority() || !OtherActor) return;

	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (Character && Character->GetCharacterMovement())
	{
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);

		FVector LaunchDir = (Character->GetActorLocation() - GetActorLocation());
		LaunchDir.Z = 0.0f; 
		LaunchDir.Normalize();

		float FinalPushForce = PushForce;
		if (Character->GetActorLocation().Z > GetActorLocation().Z + 20.0f)
		{
			FinalPushForce *= 1.3f; 
		}

		FVector LaunchVelocity = (LaunchDir * FinalPushForce) + (FVector::UpVector * UpwardForce);
		Character->LaunchCharacter(LaunchVelocity, true, true);
	}
}

void AZRotationActor::PreventClipping()
{
	TArray<AActor*> OverlappingActors;
	GetOverlappingActors(OverlappingActors, ACharacter::StaticClass());

	for (AActor* Actor : OverlappingActors)
	{
		ACharacter* Character = Cast<ACharacter>(Actor);
		if (Character)
		{
			// 캐릭터가 중심부로 파고들었다면 바깥쪽으로 강제 텔레포트
			FVector PushOutDir = Character->GetActorLocation() - GetActorLocation();
			PushOutDir.Z = 0.0f;
			PushOutDir.Normalize();

			// 액터 크기만큼 밖으로 살짝 밀어내기 (안전 거리 확보)
			FVector NewLocation = GetActorLocation() + (PushOutDir * 300.0f); // 300.f는 메쉬 크기에 맞춰 조절
			Character->SetActorLocation(NewLocation, true);
		}
	}
}