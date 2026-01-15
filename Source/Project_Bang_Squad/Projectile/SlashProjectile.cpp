#include "SlashProjectile.h"
#include "Components/BoxComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Project_Bang_Squad/Character/PaladinCharacter.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h" 
#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h"
ASlashProjectile::ASlashProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	// 1. �ڽ� ������Ʈ (���η� �а� �����ϰ�)
	BoxComp = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComp"));
	// ��: X(�������)=50, Y(�¿���)=150, Z(���ϵβ�)=10 -> 3m¥�� ����
	BoxComp->SetBoxExtent(FVector(50.f, 150.f, 10.f));

	// �浹 ���� (��ħ ���)
	BoxComp->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	BoxComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoxComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoxComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);       // ĳ���͸� ����
	BoxComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);
	BoxComp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);

	RootComponent = BoxComp;

	// 2. ��ƼŬ (���־�)
	ParticleComp = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("ParticleComp"));
	ParticleComp->SetupAttachment(RootComponent);

	// 3. �����Ʈ (���� �̵�, �߷� ����)
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = BoxComp;
	ProjectileMovement->InitialSpeed = 1500.f;  // �ӵ�
	ProjectileMovement->MaxSpeed = 1500.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->ProjectileGravityScale = 0.f; // �߷� 0 (������)

	InitialLifeSpan = 3.0f; // 3�� �� �ڵ� ����
}

void ASlashProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		BoxComp->OnComponentBeginOverlap.AddDynamic(this, &ASlashProjectile::OnOverlap);
	}
}

void ASlashProjectile::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) return;
	if (!OtherActor || OtherActor == this || OtherActor == GetOwner()) return;

	// [추가] 같은 몬스터 팀이면 충돌 처리 자체를 안 하고 리턴 (성능 절약)
	if (OtherActor->IsA(AEnemyCharacterBase::StaticClass()))
	{
		return;
	}
	if (OtherActor->IsA(ABaseCharacter::StaticClass()))
	{
		// 1. 방패에 맞았는지 확인 (팔라딘이 아니면 당연히 false 뜰 테니 안전함)
		bool bHitShield = OtherComp->GetName().Contains(TEXT("Shield"));

		// 2. 데미지 전달
		// 팔라딘이면 -> 아까 만든 TakeDamage가 돌면서 방패 피 깎음
		// 일반캐면 -> 그냥 본체 피 깎음
		UGameplayStatics::ApplyDamage(
			OtherActor,
			Damage,
			GetInstigatorController(),
			this,
			UDamageType::StaticClass()
		);

		// 3. 투사체 삭제
		// 방패에 맞았든(bHitShield), 몸에 맞았든 투사체는 사라져야 함
		Destroy();
        
		// 만약 "방패에 맞았을 때만" 팅겨나가는 소리/이펙트를 주고 싶다
		if (bHitShield)
		{
			// 방패 타격 이펙트 재생 (Multicast RPC 호출 등)
		}
	}
	// 벽에 맞았을 때
	else if (OtherComp && OtherComp->GetCollisionObjectType() == ECC_WorldStatic)
	{
		Destroy();
	}
}