// EnemyMidBoss.cpp

#include "EnemyMidBoss.h"
#include "Project_Bang_Squad/Character/MonsterBase/MidBossAIController.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Project_Bang_Squad/Projectile/SlashProjectile.h"

AEnemyMidBoss::AEnemyMidBoss()
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	CurrentPhase = EMidBossPhase::Normal;
	bReplicates = true;

	// 헬스 컴포넌트
	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));

	// 무기 충돌 박스 생성 및 설정
	WeaponCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("WeaponCollisionBox"));
	// 메시 하위에 붙임 (소켓 이름 지정)
	if (GetMesh())
	{
		WeaponCollisionBox->SetupAttachment(GetMesh(), TEXT("swordSocket"));
	}
	else
	{
		WeaponCollisionBox->SetupAttachment(RootComponent);
	}

	// 평소에는 꺼둠
	WeaponCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponCollisionBox->SetCollisionObjectType(ECC_WorldDynamic);
	WeaponCollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	WeaponCollisionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap); // 플레이어만 감지

	WeaponCollisionBox->OnComponentBeginOverlap.AddDynamic(this, &AEnemyMidBoss::OnWeaponOverlap);
}

void AEnemyMidBoss::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AEnemyMidBoss, CurrentPhase);
}

void AEnemyMidBoss::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (BossData)
	{
		if (GetMesh() && BossData->Mesh)
		{
			GetMesh()->SetSkeletalMesh(BossData->Mesh);
		}
		if (GetMesh() && BossData->AnimClass)
		{
			GetMesh()->SetAnimInstanceClass(BossData->AnimClass);
		}
		if (GetCharacterMovement())
		{
			GetCharacterMovement()->MaxWalkSpeed = BossData->WalkSpeed;
		}
	}
}

void AEnemyMidBoss::BeginPlay()
{
	Super::BeginPlay();

	// 서버 권한 및 데이터 에셋 확인
	if (HasAuthority() && BossData && HealthComponent)
	{
		HealthComponent->SetMaxHealth(BossData->MaxHealth);
		UE_LOG(LogTemp, Log, TEXT("[MidBoss] Initialized Health: %f"), BossData->MaxHealth);
	}

	// 사망 이벤트 연결
	if (HealthComponent)
	{
		HealthComponent->OnDead.AddDynamic(this, &AEnemyMidBoss::OnDeath);
	}

	// 데이터 에셋의 AttackRange를 AI에게 전달!
	if (GetController())
	{
		auto* MyAI = Cast<AMidBossAIController>(GetController());
		if (MyAI && BossData)
		{
			MyAI->SetAttackRange(BossData->AttackRange);
			UE_LOG(LogTemp, Warning, TEXT("[MidBoss] Attack Range Updated from DataAsset: %f"), BossData->AttackRange);
		}
	}
}

float AEnemyMidBoss::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (ActualDamage <= 0.0f) return 0.0f;
	if (HealthComponent && HealthComponent->IsDead()) return 0.0f;

	if (HealthComponent)
	{
		HealthComponent->ApplyDamage(ActualDamage);
	}

	if (GetController())
	{
		auto* MyAI = Cast<AMidBossAIController>(GetController());
		if (MyAI)
		{
			// [수정됨] 투사체(DamageCauser)가 사라져도 멍때리지 않게, 공격한 사람(Instigator Pawn)을 찾아서 넘겨줍니다.
			AActor* RealAttacker = DamageCauser;
			if (EventInstigator && EventInstigator->GetPawn())
			{
				RealAttacker = EventInstigator->GetPawn();
			}

			MyAI->OnDamaged(RealAttacker);
		}
	}

	return ActualDamage;
}

// --- [Combat: Weapon Collision Logic] ---

void AEnemyMidBoss::EnableWeaponCollision()
{
	if (WeaponCollisionBox)
	{
		WeaponCollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
}

void AEnemyMidBoss::DisableWeaponCollision()
{
	if (WeaponCollisionBox)
	{
		WeaponCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AEnemyMidBoss::OnWeaponOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) return;

	if (OtherActor == nullptr || OtherActor == this || OtherActor == GetOwner()) return;

	// 데미지 적용
	UGameplayStatics::ApplyDamage(
		OtherActor,
		AttackDamage,
		GetController(),
		this,
		UDamageType::StaticClass()
	);

	UE_LOG(LogTemp, Log, TEXT("BOSS: Hit Target [%s]! Damage: %f"), *OtherActor->GetName(), AttackDamage);
}

// --- [Animation Helpers] ---

float AEnemyMidBoss::PlayAggroAnim()
{
	if (!HasAuthority()) return 0.0f;
	if (BossData && BossData->AggroMontage)
	{
		// [수정] 멀티캐스트로 "모두 재생해!" 방송
		Multicast_PlayAttackMontage(BossData->AggroMontage);

		// AI에게는 애니메이션 길이 리턴 (타이머용)
		return BossData->AggroMontage->GetPlayLength();
	}
	return 0.0f;
}

// [수정됨] 서버가 몽타주를 고르고 -> 모두에게 방송 -> 길이를 리턴
float AEnemyMidBoss::PlayRandomAttack()
{
	if (!HasAuthority()) return 0.0f;

	if (BossData && BossData->AttackMontages.Num() > 0)
	{
		int32 RandomIndex = FMath::RandRange(0, BossData->AttackMontages.Num() - 1);
		UAnimMontage* SelectedMontage = BossData->AttackMontages[RandomIndex];

		if (SelectedMontage)
		{
			// 멀티캐스트 호출 (클라이언트들도 재생)
			Multicast_PlayAttackMontage(SelectedMontage);

			// AI 타이머용 길이 리턴
			return SelectedMontage->GetPlayLength();
		}
	}
	return 0.0f;
}

// [NEW] 멀티캐스트 구현부
void AEnemyMidBoss::Multicast_PlayAttackMontage_Implementation(UAnimMontage* MontageToPlay)
{
	if (MontageToPlay)
	{
		PlayAnimMontage(MontageToPlay);
	}
}

// EnemyMidBoss.cpp

float AEnemyMidBoss::PlayHitReactAnim()
{
	// 1. 권한 체크 (서버만 실행)
	if (!HasAuthority()) return 0.0f;

	if (BossData && BossData->HitReactMontage)
	{
		// 2. [수정] 멀티캐스트로 "모두 아픈 모션 재생해!" 방송
		Multicast_PlayAttackMontage(BossData->HitReactMontage);

		// 3. AI에게는 애니메이션 길이 리턴 (스턴 시간 계산용)
		return BossData->HitReactMontage->GetPlayLength();
	}
	return 0.0f;
}

// --- [Phase Control] ---

void AEnemyMidBoss::SetPhase(EMidBossPhase NewPhase)
{
	if (HasAuthority())
	{
		CurrentPhase = NewPhase;
		OnRep_CurrentPhase();
	}
}

void AEnemyMidBoss::OnRep_CurrentPhase()
{
	switch (CurrentPhase)
	{
	case EMidBossPhase::Normal:
		break;
	case EMidBossPhase::Gimmick:
		UE_LOG(LogTemp, Warning, TEXT("[MidBoss] Phase GIMMICK Start! (FX)"));
		break;
	case EMidBossPhase::Dead:
		// 사망 애니메이션
		if (BossData && BossData->DeathMontage)
		{
			PlayAnimMontage(BossData->DeathMontage);
		}

		// 충돌 해제
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// 무기 판정도 확실히 끄기
		DisableWeaponCollision();
		break;
	}
}

void AEnemyMidBoss::OnDeath()
{
	SetPhase(EMidBossPhase::Dead);

	auto* MyAI = Cast<AMidBossAIController>(GetController());
	if (MyAI)
	{
		MyAI->SetDeadState();
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DisableWeaponCollision();

	if (HasAuthority())
	{
		SetLifeSpan(5.0f);
	}

	UE_LOG(LogTemp, Warning, TEXT("BOSS IS DEAD. Cleanup started."));
}

// 1. 패턴 시작 (AI가 호출)
void AEnemyMidBoss::PlaySlashPattern()
{
	if (BossData && BossData->SlashAttackMontage)
	{
		PlayAnimMontage(BossData->SlashAttackMontage);
	}
}

// 2. 발사 트리거 (AnimNotify에서 호출)
void AEnemyMidBoss::FireSlashProjectile()
{
	if (!BossData || !BossData->SlashProjectileClass) return;

	FVector SpawnLoc = GetActorLocation() + GetActorForwardVector() * 100.f;
	FRotator SpawnRot = GetActorRotation();

	if (GetMesh()->DoesSocketExist(TEXT("swordSocket")))
	{
		SpawnLoc = GetMesh()->GetSocketLocation(TEXT("swordSocket"));
	}

	// 서버에게 요청
	Server_SpawnSlashProjectile(SpawnLoc, SpawnRot);
}

// 3. 서버 구현부
void AEnemyMidBoss::Server_SpawnSlashProjectile_Implementation(FVector SpawnLoc, FRotator SpawnRot)
{
	if (!BossData || !BossData->SlashProjectileClass) return;

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;

	GetWorld()->SpawnActor<AActor>(BossData->SlashProjectileClass, SpawnLoc, SpawnRot, Params);
}