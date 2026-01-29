#include "SeaMonster.h"
#include "SeaMonsterProjectile.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/ProjectileMovementComponent.h"

ASeaMonster::ASeaMonster()
{
	PrimaryActorTick.bCanEverTick = true;
	MeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;

	ProjectileSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("ProjectileSpawnPoint"));
	ProjectileSpawnPoint->SetupAttachment(RootComponent);
}

void ASeaMonster::BeginPlay()
{
	Super::BeginPlay();
	GetWorldTimerManager().SetTimer(TimerHandle_Attack, this, &ASeaMonster::Fire, FireInterval, true);
}

void ASeaMonster::Fire()
{
	if (!ProjectileClass || !ProjectileSpawnPoint) return;

	// 1. 발사 위치와 방향 가져오기 (컴포넌트 기준)
	FVector SpawnLocation = ProjectileSpawnPoint->GetComponentLocation();
	FVector ForwardDir = ProjectileSpawnPoint->GetForwardVector();

	// 2. 목표 지점 계산: 컴포넌트 정면 방향으로 FireDistance만큼 떨어진 곳
	FVector TargetPos = SpawnLocation + (ForwardDir * FireDistance);

	FVector OutLaunchVelocity;
	// 3. 직선 느낌을 위해 LaunchArc 값을 사용해 속도 계산
	bool bHaveSolution = UGameplayStatics::SuggestProjectileVelocity_CustomArc(
		this, OutLaunchVelocity, SpawnLocation, TargetPos, GetWorld()->GetGravityZ(), LaunchArc
	);

	if (bHaveSolution)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;

		ASeaMonsterProjectile* Projectile = GetWorld()->SpawnActor<ASeaMonsterProjectile>(
			ProjectileClass, SpawnLocation, OutLaunchVelocity.Rotation(), SpawnParams
		);

		if (Projectile && Projectile->GetProjectileMovement())
		{
			Projectile->GetProjectileMovement()->Velocity = OutLaunchVelocity;
		}
	}
}

void ASeaMonster::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if WITH_EDITOR
	if (!GetWorld()->IsGameWorld() && ProjectileSpawnPoint)
	{
		// 에디터 뷰포트에서 낙하 지점을 빨간색 구체로 미리 보여줍니다.
		FVector SpawnLocation = ProjectileSpawnPoint->GetComponentLocation();
		FVector ForwardDir = ProjectileSpawnPoint->GetForwardVector();
		FVector TargetPos = SpawnLocation + (ForwardDir * FireDistance);

		DrawDebugSphere(GetWorld(), TargetPos, 50.f, 12, FColor::Red, false, -1, 0, 2.f);
	}
#endif
}