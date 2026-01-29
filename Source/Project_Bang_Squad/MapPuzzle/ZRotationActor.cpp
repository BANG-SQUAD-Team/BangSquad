#include "ZRotationActor.h"
#include "GameFramework/Character.h"

AZRotationActor::AZRotationActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// 1. 컴포넌트 먼저 생성 (순서 필수!)
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;

	// 2. 네트워크 복제 및 물리 설정
	bReplicates = true;
	SetReplicateMovement(true);

	MeshComponent->SetNotifyRigidBodyCollision(true);
	MeshComponent->SetCollisionProfileName(TEXT("BlockAll"));
}

void AZRotationActor::BeginPlay()
{
	Super::BeginPlay();

	// 히트 이벤트 바인딩
	if (MeshComponent)
	{
		MeshComponent->OnComponentHit.AddDynamic(this, &AZRotationActor::OnMeshHit);
	}
}

void AZRotationActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HasAuthority()) // 서버에서만 회전 계산
	{
		float DirectionMultiplier = bClockwise ? 1.0f : -1.0f;
		FRotator NewRotation = GetActorRotation();
		NewRotation.Yaw += (RotationSpeed * DirectionMultiplier * DeltaTime);

		SetActorRotation(NewRotation);
	}
}

void AZRotationActor::OnMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 서버에서만 물리 효과 적용
	if (!HasAuthority() || !OtherActor) return;

	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (Character)
	{
		// 방향 벡터 계산: $\vec{V}_{launch} = \text{Normal}(\vec{P}_{char} - \vec{P}_{actor}) \times F_{push} + (0, 0, F_{up})$
		FVector LaunchDir = (Character->GetActorLocation() - GetActorLocation()).GetSafeNormal();

		// 수평 힘과 수직 띄우기 힘 합치기
		FVector LaunchVelocity = (LaunchDir * PushForce) + (FVector::UpVector * UpwardForce);

		// 캐릭터 발사 (Override 설정으로 확실하게 밀어냄)
		Character->LaunchCharacter(LaunchVelocity, true, true);
	}
}