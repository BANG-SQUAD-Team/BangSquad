#include "SlashProjectile.h"
#include "Components/BoxComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h" // 플레이어 헤더 확인

ASlashProjectile::ASlashProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	// 1. 박스 컴포넌트 (가로로 넓고 납작하게)
	BoxComp = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComp"));
	// 예: X(진행방향)=50, Y(좌우폭)=150, Z(상하두께)=10 -> 3m짜리 참격
	BoxComp->SetBoxExtent(FVector(50.f, 150.f, 10.f));

	// 충돌 설정 (겹침 허용)
	BoxComp->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	BoxComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoxComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoxComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);       // 캐릭터만 감지
	BoxComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);  // 벽은 감지

	RootComponent = BoxComp;

	// 2. 파티클 (비주얼)
	ParticleComp = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("ParticleComp"));
	ParticleComp->SetupAttachment(RootComponent);

	// 3. 무브먼트 (직선 이동, 중력 없음)
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = BoxComp;
	ProjectileMovement->InitialSpeed = 1500.f;  // 속도
	ProjectileMovement->MaxSpeed = 1500.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->ProjectileGravityScale = 0.f; // 중력 0 (일직선)

	InitialLifeSpan = 3.0f; // 3초 뒤 자동 삭제
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
	if (!HasAuthority()) return; // 서버만 처리
	if (!OtherActor || OtherActor == this || OtherActor == GetOwner()) return;

	// 플레이어만 타격
	if (OtherActor->IsA(ABaseCharacter::StaticClass()))
	{
		// 1. 데미지 적용
		UGameplayStatics::ApplyDamage(
			OtherActor,
			Damage,
			GetInstigatorController(), // 보스의 컨트롤러
			this, // 데미지 가해자는 '투사체' (이걸로 아까 AI 문제를 수정했었죠?)
			UDamageType::StaticClass()
		);

		// 2. [선택] 관통 여부
		// 참격은 보통 뚫고 지나가는 게 멋있으므로 Destroy()를 안 씁니다.
		// 대신 다단히트가 걱정된다면 Destroy()를 넣으세요.
		// Destroy(); 
	}
	// 벽에 닿으면 삭제
	else if (OtherComp && OtherComp->GetCollisionObjectType() == ECC_WorldStatic)
	{
		Destroy();
	}
}