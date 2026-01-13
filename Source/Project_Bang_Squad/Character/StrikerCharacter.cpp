#include "Project_Bang_Squad/Character/StrikerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"

AStrikerCharacter::AStrikerCharacter()
{
	// 기동성 확보
	GetCharacterMovement()->MaxWalkSpeed = 750.f;
	bIsSlamming = false;
}

void AStrikerCharacter::BeginPlay()
{
	Super::BeginPlay();
}

// =============================================================
// [착지 판정] Landed 오버라이드
// =============================================================
void AStrikerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	// 스킬 2번(찍기) 상태로 착지했을 때만 로직 실행
	if (bIsSlamming)
	{
		bIsSlamming = false;
		Server_Skill2Impact();
	}
}

void AStrikerCharacter::Attack()
{
	ProcessSkill(TEXT("Attack"));
}

void AStrikerCharacter::Skill1()
{
	ProcessSkill(TEXT("Skill1"));
}

// =============================================================
// [스킬 2] 공중 찍기 (사용 시 급강하)
// =============================================================
void AStrikerCharacter::Skill2()
{
	// 공중에 있을 때만 사용 가능
	if (GetCharacterMovement()->IsFalling())
	{
		ProcessSkill(TEXT("Skill2"));

		// 1. 바닥으로 강하게 찍기 (Z축 하강)
		FVector SlamVelocity = FVector(0.f, 0.f, -3000.f);
		LaunchCharacter(SlamVelocity, true, true);

		// 2. 찍기 중임을 표시 (Landed에서 확인)
		bIsSlamming = true;
	}
	else
	{
		// 지상 사용 불가 (필요 시 점프 로직 추가 가능)
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Red, TEXT("Only usable in Air!"));
	}
}

// =============================================================
// [서버] 찍기 충돌 처리 (적은 당기고, 아군은 밀침)
// =============================================================
void AStrikerCharacter::Server_Skill2Impact_Implementation()
{
	FVector MyLoc = GetActorLocation();

	// 1. 이펙트 재생
	Multicast_PlaySlamFX();

	// 2. 범위 내 캐릭터 탐색 (반경 600)
	TArray<AActor*> OverlappingActors;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

	UKismetSystemLibrary::SphereOverlapActors(GetWorld(), MyLoc, 600.f,
		ObjectTypes, ACharacter::StaticClass(), { this }, OverlappingActors);

	float SlamDamage = 50.f;
	if (SkillDataTable)
	{
		static const FString ContextString(TEXT("StrikerSkill2"));
		FSkillData* Data = SkillDataTable->FindRow<FSkillData>(TEXT("Skill2"), ContextString);
		if (Data) SlamDamage = Data->Damage;
	}

	// 3. 대상 처리
	for (AActor* Actor : OverlappingActors)
	{
		ACharacter* TargetChar = Cast<ACharacter>(Actor);
		if (!TargetChar || TargetChar == this) continue;

		// [구분 로직] BaseCharacter 상속 여부로 아군/적군 판단
		if (TargetChar->IsA(ABaseCharacter::StaticClass()))
		{
			// === [아군] 밀쳐내기 (Push) ===
			FVector PushDir = (TargetChar->GetActorLocation() - MyLoc).GetSafeNormal();
			FVector PushVel = (PushDir * 800.f) + FVector(0.f, 0.f, 200.f); // 살짝 띄워서 밀기
			TargetChar->LaunchCharacter(PushVel, true, true);
		}
		else
		{
			// === [적] 당겨오기 (Pull) + 데미지 ===
			UGameplayStatics::ApplyDamage(TargetChar, SlamDamage, GetController(), this, UDamageType::StaticClass());

			FVector PullDir = (MyLoc - TargetChar->GetActorLocation()).GetSafeNormal();
			FVector PullVel = (PullDir * 1500.f) + FVector(0.f, 0.f, 300.f); // 살짝 띄워서 당기기
			TargetChar->LaunchCharacter(PullVel, true, true);
		}
	}
}

void AStrikerCharacter::Multicast_PlaySlamFX_Implementation()
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Purple, TEXT("SLAM IMPACT!!"));
}

// =============================================================
// [직업 능력] 야스오 궁극기
// =============================================================
void AStrikerCharacter::JobAbility()
{
	AActor* Target = FindBestAirborneTarget();
	if (Target)
	{
		Server_TryJobAbility(Target);
	}
	else
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Red, TEXT("No Airborne Target!"));
	}
}

AActor* AStrikerCharacter::FindBestAirborneTarget()
{
	FVector MyLoc = GetActorLocation();
	FVector CamFwd = GetControlRotation().Vector();

	TArray<AActor*> OverlappingActors;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

	UKismetSystemLibrary::SphereOverlapActors(GetWorld(), MyLoc, 1200.f,
		ObjectTypes, ACharacter::StaticClass(), { this }, OverlappingActors);

	AActor* BestTarget = nullptr;
	float BestDot = -1.0f;

	for (AActor* Actor : OverlappingActors)
	{
		ACharacter* Enemy = Cast<ACharacter>(Actor);
		if (!Enemy) continue;

		// 아군(BaseCharacter)은 타겟 제외
		if (Enemy->IsA(ABaseCharacter::StaticClass())) continue;

		if (Enemy->GetCharacterMovement()->IsFalling())
		{
			FVector DirToEnemy = (Enemy->GetActorLocation() - MyLoc).GetSafeNormal();
			float DotResult = FVector::DotProduct(CamFwd, DirToEnemy);

			if (DotResult > 0.5f && DotResult > BestDot)
			{
				BestDot = DotResult;
				BestTarget = Enemy;
			}
		}
	}
	return BestTarget;
}

void AStrikerCharacter::Server_TryJobAbility_Implementation(AActor* TargetActor)
{
	ACharacter* TargetChar = Cast<ACharacter>(TargetActor);
	if (!TargetChar) return;

	float DistSq = FVector::DistSquared(GetActorLocation(), TargetActor->GetActorLocation());
	if (DistSq > 1500.f * 1500.f) return;

	float SkillDamage = 0.f;
	if (SkillDataTable)
	{
		static const FString ContextString(TEXT("Striker JobAbility Context"));
		FSkillData* Data = SkillDataTable->FindRow<FSkillData>(TEXT("JobAbility"), ContextString);

		if (Data)
		{
			if (!IsSkillUnlocked(Data->RequiredStage)) return;
			SkillDamage = Data->Damage;
			if (Data->SkillMontage) Multicast_PlayJobAbilityFX(TargetChar);
		}
	}

	FVector TeleportLoc = TargetChar->GetActorLocation() - (TargetChar->GetActorForwardVector() * 100.f) + FVector(0, 0, 50.f);
	SetActorLocation(TeleportLoc);

	FVector LookDir = TargetChar->GetActorLocation() - TeleportLoc;
	LookDir.Z = 0.f;
	SetActorRotation(LookDir.Rotation());

	SuspendTarget(TargetChar);
	UGameplayStatics::ApplyDamage(TargetChar, SkillDamage, GetController(), this, UDamageType::StaticClass());

	GetCharacterMovement()->GravityScale = 0.f;
	GetCharacterMovement()->Velocity = FVector::ZeroVector;

	FTimerHandle Handle;
	GetWorld()->GetTimerManager().SetTimer(Handle, [this, TargetChar]()
		{
			GetCharacterMovement()->GravityScale = 1.0f;
			ReleaseTarget(TargetChar);
		}, 1.0f, false);
}

void AStrikerCharacter::SuspendTarget(ACharacter* Target)
{
	if (!Target) return;
	UCharacterMovementComponent* EMC = Target->GetCharacterMovement();
	if (EMC)
	{
		EMC->GravityScale = 0.f;
		EMC->Velocity = FVector::ZeroVector;
		EMC->SetMovementMode(MOVE_Flying);
	}
}

void AStrikerCharacter::ReleaseTarget(ACharacter* Target)
{
	if (Target)
	{
		UCharacterMovementComponent* EMC = Target->GetCharacterMovement();
		if (EMC)
		{
			EMC->GravityScale = 1.0f;
			EMC->SetMovementMode(MOVE_Walking);
		}
	}
}

void AStrikerCharacter::Multicast_PlayJobAbilityFX_Implementation(AActor* Target)
{
	ProcessSkill(TEXT("JobAbility"));
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, TEXT("Sorye Ge Ton!!"));
}

void AStrikerCharacter::ProcessSkill(FName SkillRowName)
{
	if (!SkillDataTable) return;

	static const FString ContextString(TEXT("StrikerSkillContext"));
	FSkillData* Data = SkillDataTable->FindRow<FSkillData>(SkillRowName, ContextString);

	if (Data)
	{
		if (!IsSkillUnlocked(Data->RequiredStage))
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Orange, TEXT("Skill is locked."));
			return;
		}

		if (Data->SkillMontage)
		{
			PlayAnimMontage(Data->SkillMontage);
		}
	}
}