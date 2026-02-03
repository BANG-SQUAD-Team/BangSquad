#include "ZRotationActor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/RotatingMovementComponent.h"

AZRotationActor::AZRotationActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// 1. [NEW] 진짜 회전축(SceneComponent) 생성 및 루트 설정
	DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	RootComponent = DefaultSceneRoot; // 이제 얘가 회전의 중심입니다.

	// 2. 박스 컴포넌트 생성 (루트의 자식으로 붙임)
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetupAttachment(DefaultSceneRoot);

	CollisionBox->SetBoxExtent(FVector(50.f, 50.f, 50.f));
	CollisionBox->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	CollisionBox->SetNotifyRigidBodyCollision(true);
	CollisionBox->CanCharacterStepUpOn = ECB_No;

	// 3. 메쉬 컴포넌트 생성 (루트의 자식으로 붙임 - 박스와 형제 관계)
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(DefaultSceneRoot);
	// 메쉬 위치를 박스와 별개로 자유롭게 움직일 수 있게 됨

	MeshComponent->SetCollisionProfileName(TEXT("NoCollision")); // 충돌은 박스에게 맡김

	// 4. 회전 컴포넌트
	RotatingMovement = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotatingMovement"));

	// ★ 중요: 이제 박스가 아니라 '루트(축)'를 돌려야 함
	RotatingMovement->SetUpdatedComponent(DefaultSceneRoot);

	// 5. 네트워크
	bReplicates = true;
	SetReplicateMovement(true);
}

void AZRotationActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (RotatingMovement)
	{
		float Direction = bClockwise ? 1.0f : -1.0f;
		RotatingMovement->RotationRate = FRotator(0.0f, RotationSpeed * Direction, 0.0f);
	}
}

void AZRotationActor::BeginPlay()
{
	Super::BeginPlay();

	if (CollisionBox)
	{
		CollisionBox->OnComponentHit.AddDynamic(this, &AZRotationActor::OnHit);
	}
}

void AZRotationActor::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority() || !OtherActor) return;

	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (Character && Character->GetCharacterMovement())
	{
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);

		// 중심축(Root) 기준으로 밀어내는 게 가장 안정적입니다.
		FVector KnockbackDir = (Character->GetActorLocation() - GetActorLocation());
		KnockbackDir.Z = 0.0f;
		KnockbackDir.Normalize();

		FVector LaunchVelocity = (KnockbackDir * PushForce) + (FVector::UpVector * UpwardForce);
		Character->LaunchCharacter(LaunchVelocity, true, true);
	}
}