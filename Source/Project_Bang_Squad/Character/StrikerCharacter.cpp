#include "Project_Bang_Squad/Character/StrikerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"
#include "Project_Bang_Squad/Character/Enemy/EnemyNormal.h"
#include "Project_Bang_Squad/Character/Enemy/EnemyMidBoss.h"

AStrikerCharacter::AStrikerCharacter()
{
	bIsSlamming = false;
}

void AStrikerCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void AStrikerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (bIsSlamming)
	{
		bIsSlamming = false;
		Server_Skill2Impact();
	}
}

// =============================================================
// [평타] 겟앰프드 스타일 (데이터 테이블 쿨타임 적용)
// =============================================================
void AStrikerCharacter::Attack()
{
	// 1. 쿨타임 및 상태 확인
	if (!CanAttack()) return;

	// 2. 데이터 테이블에서 쿨타임 설정
	if (SkillDataTable)
	{
		static const FString ContextString(TEXT("StrikerAttack"));
		FSkillData* Row = SkillDataTable->FindRow<FSkillData>(TEXT("Attack"), ContextString);
		// 데이터 테이블에 값이 있고 0보다 크면 그 값으로 쿨타임 갱신
		if (Row && Row->Cooldown > 0.0f)
		{
			AttackCooldownTime = Row->Cooldown;
		}
	}

	// 3. 쿨타임 시작 (BaseCharacter 기능)
	StartAttackCooldown();

	// 4. 실행
	ProcessSkill(TEXT("Attack"));

	FVector ForwardDir = GetActorForwardVector();
	FVector LaunchVel = ForwardDir * AttackForwardForce;
	LaunchCharacter(LaunchVel, true, false);
}

// =============================================================
// [스킬 1] 야스오 궁 (데이터 테이블 쿨타임 적용)
// =============================================================
void AStrikerCharacter::Skill1()
{
	// 1. 쿨타임 체크 (현재 시간이 준비 시간보다 작으면 쿨타임 중)
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime < Skill1ReadyTime)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, TEXT("Skill1 Cooldown!"));
		return;
	}

	// [로그 1] 키 입력 확인
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, TEXT(">> [Skill1] Input Pressed!"));

	AActor* Target = FindBestAirborneTarget();
	if (Target)
	{
		// 2. 쿨타임 적용 (데이터 테이블 조회)
		float ActualCooldown = 0.0f; // 기본값
		if (SkillDataTable)
		{
			static const FString ContextString(TEXT("StrikerSkill1Cooldown"));
			FSkillData* Data = SkillDataTable->FindRow<FSkillData>(TEXT("Skill1"), ContextString);
			if (Data && Data->Cooldown > 0.0f)
			{
				ActualCooldown = Data->Cooldown;
			}
		}

		// 쿨타임 적용: 현재 시간 + 쿨타임
		Skill1ReadyTime = CurrentTime + ActualCooldown;

		// [로그 2] 타겟 발견 성공
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, FString::Printf(TEXT(">> [Skill1] Client: Found Target [%s]"), *Target->GetName()));
		Server_TrySkill1(Target);
	}
	else
	{
		// [로그 3] 타겟 없음
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT(">> [Skill1] Client: No Airborne Target Found!"));
	}
}

AActor* AStrikerCharacter::FindBestAirborneTarget()
{
	FVector MyLoc = GetActorLocation();
	FVector CamFwd = GetControlRotation().Vector();
	CamFwd.Z = 0.f;
	CamFwd.Normalize();

	TArray<AActor*> OverlappingActors;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

	UKismetSystemLibrary::SphereOverlapActors(GetWorld(), MyLoc, 1200.f,
		ObjectTypes, ACharacter::StaticClass(), { this }, OverlappingActors);

	AActor* BestTarget = nullptr;
	float BestDot = -1.0f;

	for (AActor* Actor : OverlappingActors)
	{
		ACharacter* CharActor = Cast<ACharacter>(Actor);
		if (!CharActor) continue;

		bool bIsNormal = Actor->IsA(AEnemyNormal::StaticClass());
		bool bIsMidBoss = Actor->IsA(AEnemyMidBoss::StaticClass());

		// 적군(Normal, MidBoss)만 대상
		if (!bIsNormal && !bIsMidBoss) continue;

		bool bIsFalling = CharActor->GetCharacterMovement()->IsFalling();

		if (bIsFalling)
		{
			float HeightDiff = CharActor->GetActorLocation().Z - MyLoc.Z;

			if (HeightDiff < Skill1RequiredHeight)
			{
				continue;
			}

			FVector DirToEnemy = (CharActor->GetActorLocation() - MyLoc);
			DirToEnemy.Z = 0.f;
			DirToEnemy.Normalize();

			float DotResult = FVector::DotProduct(CamFwd, DirToEnemy);

			if (DotResult > 0.5f && DotResult > BestDot)
			{
				BestDot = DotResult;
				BestTarget = CharActor;
			}
		}
	}

	return BestTarget;
}

void AStrikerCharacter::Server_TrySkill1_Implementation(AActor* TargetActor)
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Blue, TEXT(">> [Server] Request Received"));

	ACharacter* TargetChar = Cast<ACharacter>(TargetActor);
	if (!TargetChar) return;

	float DistSq = FVector::DistSquared(GetActorLocation(), TargetActor->GetActorLocation());
	if (DistSq > 1500.f * 1500.f)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT(">> [Server] Target too far!"));
		return;
	}

	float SkillDamage = 0.f;

	if (SkillDataTable)
	{
		static const FString ContextString(TEXT("Striker Skill1 Context"));
		FSkillData* Data = SkillDataTable->FindRow<FSkillData>(TEXT("Skill1"), ContextString);

		if (Data)
		{
			if (!IsSkillUnlocked(Data->RequiredStage))
			{
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT(">> [Server] Skill Locked!"));
				return;
			}
			SkillDamage = Data->Damage;
			if (Data->SkillMontage) Multicast_PlaySkill1FX(TargetChar);
		}
		else
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT(">> [Server] 'Skill1' Row Not Found in DataTable!"));
		}
	}
	else
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT(">> [Server] SkillDataTable is NULL!"));
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

void AStrikerCharacter::Multicast_PlaySkill1FX_Implementation(AActor* Target)
{
	ProcessSkill(TEXT("Skill1"));
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, TEXT("Skill1: Sorye Ge Ton!!"));
}

// =============================================================
// [직업 능력] 전방 직사각형 범위 띄우기 (데이터 테이블 쿨타임 적용)
// =============================================================
void AStrikerCharacter::JobAbility()
{
	// 1. 쿨타임 체크
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime < JobAbilityCooldownTime)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Silver, TEXT("Job Ability Cooldown!"));
		return;
	}

	// 2. 쿨타임 값 가져오기
	float ActualCooldown = 5.0f; // 기본값
	if (SkillDataTable)
	{
		static const FString ContextString(TEXT("StrikerJobCooldown"));
		FSkillData* Data = SkillDataTable->FindRow<FSkillData>(TEXT("JobAbility"), ContextString);
		if (Data && Data->Cooldown > 0.0f)
		{
			ActualCooldown = Data->Cooldown;
		}
	}

	Server_UseJobAbility();

	// 3. 쿨타임 적용
	JobAbilityCooldownTime = CurrentTime + ActualCooldown;
}

void AStrikerCharacter::Server_UseJobAbility_Implementation()
{
	float AbilityDamage = 50.f;
	ProcessSkill(TEXT("JobAbility"));

	if (SkillDataTable)
	{
		static const FString ContextString(TEXT("Striker JobAbility Damage"));
		FSkillData* Data = SkillDataTable->FindRow<FSkillData>(TEXT("JobAbility"), ContextString);
		if (Data) AbilityDamage = Data->Damage;
	}

	FVector MyLoc = GetActorLocation();
	FVector MyFwd = GetActorForwardVector();
	FVector BoxCenter = MyLoc + (MyFwd * 150.f);
	FVector BoxExtent = FVector(150.f, 100.f, 100.f);

	TArray<AActor*> OverlappingActors;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

	UKismetSystemLibrary::BoxOverlapActors(GetWorld(), BoxCenter, BoxExtent, ObjectTypes, ACharacter::StaticClass(), { this }, OverlappingActors);

	for (AActor* Actor : OverlappingActors)
	{
		ACharacter* TargetChar = Cast<ACharacter>(Actor);
		if (!TargetChar) continue;

		bool bIsNormal = Actor->IsA(AEnemyNormal::StaticClass());
		bool bIsMidBoss = Actor->IsA(AEnemyMidBoss::StaticClass());
		bool bIsBaseChar = Actor->IsA(ABaseCharacter::StaticClass());

		if (bIsNormal)
		{
			UGameplayStatics::ApplyDamage(TargetChar, AbilityDamage, GetController(), this, UDamageType::StaticClass());
			FVector LaunchVel = FVector(0.f, 0.f, 1000.f);
			TargetChar->LaunchCharacter(LaunchVel, true, true);
		}
		else if (bIsBaseChar && !bIsNormal && !bIsMidBoss)
		{
			FVector LaunchVel = FVector(0.f, 0.f, 1000.f);
			TargetChar->LaunchCharacter(LaunchVel, true, true);
		}
	}
}

// =============================================================
// [스킬 2] 공중 찍기 (데이터 테이블 쿨타임 적용)
// =============================================================
void AStrikerCharacter::Skill2()
{
	// 1. 쿨타임 체크
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime < Skill2ReadyTime)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, TEXT("Skill2 Cooldown!"));
		return;
	}

	if (GetCharacterMovement()->IsFalling())
	{
		// 2. 쿨타임 적용 (실행 성공 시에만)
		float ActualCooldown = 0.0f;
		if (SkillDataTable)
		{
			static const FString ContextString(TEXT("StrikerSkill2Cooldown"));
			FSkillData* Data = SkillDataTable->FindRow<FSkillData>(TEXT("Skill2"), ContextString);
			if (Data && Data->Cooldown > 0.0f)
			{
				ActualCooldown = Data->Cooldown;
			}
		}
		// 다음 사용 가능 시간 = 현재 시간 + 쿨타임
		Skill2ReadyTime = CurrentTime + ActualCooldown;

		ProcessSkill(TEXT("Skill2"));
		FVector SlamVelocity = FVector(0.f, 0.f, -3000.f);
		LaunchCharacter(SlamVelocity, true, true);
		bIsSlamming = true;
	}
	else
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Red, TEXT("Only usable in Air!"));
	}
}

void AStrikerCharacter::Server_Skill2Impact_Implementation()
{
	FVector MyLoc = GetActorLocation();
	Multicast_PlaySlamFX();

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

	for (AActor* Actor : OverlappingActors)
	{
		ACharacter* TargetChar = Cast<ACharacter>(Actor);
		if (!TargetChar || TargetChar == this) continue;

		bool bIsNormal = Actor->IsA(AEnemyNormal::StaticClass());
		bool bIsMidBoss = Actor->IsA(AEnemyMidBoss::StaticClass());
		bool bIsBaseChar = Actor->IsA(ABaseCharacter::StaticClass());

		if (bIsNormal)
		{
			UGameplayStatics::ApplyDamage(TargetChar, SlamDamage, GetController(), this, UDamageType::StaticClass());
			FVector PullDir = (MyLoc - TargetChar->GetActorLocation()).GetSafeNormal();
			FVector PullVel = (PullDir * 1500.f) + FVector(0.f, 0.f, 300.f);
			TargetChar->LaunchCharacter(PullVel, true, true);
		}
		else if (bIsBaseChar && !bIsMidBoss)
		{
			FVector PushDir = (TargetChar->GetActorLocation() - MyLoc).GetSafeNormal();
			FVector PushVel = (PushDir * 800.f) + FVector(0.f, 0.f, 200.f);
			TargetChar->LaunchCharacter(PushVel, true, true);
		}
	}
}

void AStrikerCharacter::Multicast_PlaySlamFX_Implementation()
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Purple, TEXT("SLAM IMPACT!!"));
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