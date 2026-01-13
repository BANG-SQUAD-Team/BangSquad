// EnemyMidBoss.cpp

#include "EnemyMidBoss.h"
#include "Project_Bang_Squad/Character/MonsterBase/MidBossAIController.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
// [NEW] 필수 헤더
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"

AEnemyMidBoss::AEnemyMidBoss()
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	CurrentPhase = EMidBossPhase::Normal;
	bReplicates = true;

	// 헬스 컴포넌트
	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));

	// 무기 충돌 박스 생성 및 설정
	WeaponCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("WeaponCollisionBox"));
	WeaponCollisionBox->SetupAttachment(GetMesh()); // 메시 하위에 붙임
	if (GetMesh())
	{
		WeaponCollisionBox->SetupAttachment(GetMesh(), TEXT("swordSocket"));
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
	//서버 권한 및 데이터 에셋 확인
	if (HasAuthority() && BossData && HealthComponent)
	{
		HealthComponent->SetMaxHealth(BossData->MaxHealth);
		UE_LOG(LogTemp, Log, TEXT("[MidBoss] Initialized Health: %f"), BossData->MaxHealth);
	}
	//사망 이벤트 연결
	if (HealthComponent)
	{
		HealthComponent->OnDead.AddDynamic(this, &AEnemyMidBoss::OnDeath);
	}
	//데이터 에셋의 AttackRange를 AI에게 전달!
	if (GetController())
	{
		auto* MyAI = Cast<AMidBossAIController>(GetController());
		if (MyAI && BossData)
		{
			// 데이터 에셋의 값을 컨트롤러에 덮어씌움
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
			MyAI->OnDamaged(DamageCauser);
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

	// [옵션] 다단히트 방지 (필요 시 주석 해제)
	// DisableWeaponCollision(); 
}

// --- [Animation Helpers] ---

float AEnemyMidBoss::PlayAggroAnim()
{
	if (BossData && BossData->AggroMontage)
	{
		return PlayAnimMontage(BossData->AggroMontage);
	}
	return 0.0f;
}

float AEnemyMidBoss::PlayRandomAttack()
{
	if (BossData && BossData->AttackMontages.Num() > 0)
	{
		int32 RandomIndex = FMath::RandRange(0, BossData->AttackMontages.Num() - 1);
		UAnimMontage* SelectedMontage = BossData->AttackMontages[RandomIndex];
		if (SelectedMontage)
		{
			return PlayAnimMontage(SelectedMontage);
		}
	}
	return 0.0f;
}

float AEnemyMidBoss::PlayHitReactAnim()
{
	if (BossData && BossData->HitReactMontage)
	{
		return PlayAnimMontage(BossData->HitReactMontage);
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