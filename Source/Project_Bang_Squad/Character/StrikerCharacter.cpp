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

// =================================================================
// [생명주기 및 오버라이드]
// =================================================================

void AStrikerCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void AStrikerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	// 스킬2(찍기) 사용 중 착지하면 충격파 발생
	if (bIsSlamming)
	{
		bIsSlamming = false;
		Server_Skill2Impact();
	}
}

void AStrikerCharacter::OnDeath()
{
	if (bIsDead) return;

	// Skill 1 정리: 공중에 띄워둔 적 해방
	if (CurrentComboTarget)
	{
		ReleaseTarget(CurrentComboTarget);
		CurrentComboTarget = nullptr;
	}

	GetWorldTimerManager().ClearTimer(Skill1TimerHandle);

	// Skill 2 정리
	bIsSlamming = false;

	StopAnimMontage();
	Super::OnDeath();
}

// =================================================================
// [입력 핸들러 및 평타]
// =================================================================

void AStrikerCharacter::Attack()
{
	if (!CanAttack()) return;

	FName SkillRowName = bIsNextAttackA ? TEXT("Attack_A") : TEXT("Attack_B");

	// 서버에 공격 요청 (동기화)
	Server_Attack(SkillRowName);

	bIsNextAttackA = !bIsNextAttackA;
}

void AStrikerCharacter::Server_Attack_Implementation(FName SkillName)
{
	// 쿨타임 데이터 적용 (서버)
	if (SkillDataTable)
	{
		static const FString ContextString(TEXT("StrikerAttack"));
		FSkillData* Row = SkillDataTable->FindRow<FSkillData>(SkillName, ContextString);
		if (Row && Row->Cooldown > 0.0f)
		{
			AttackCooldownTime = Row->Cooldown;
		}
	}
	StartAttackCooldown();

	// 모든 클라이언트에게 모션 재생 명령
	Multicast_Attack(SkillName);
}

void AStrikerCharacter::Multicast_Attack_Implementation(FName SkillName)
{
	// 실제 모션 재생
	ProcessSkill(SkillName);
}

void AStrikerCharacter::ApplyAttackForwardForce()
{
	// 클라이언트에서 호출 시 서버로 중계
	if (HasAuthority())
	{
		Server_ApplyAttackForwardForce_Implementation();
	}
	else
	{
		Server_ApplyAttackForwardForce();
	}
}

void AStrikerCharacter::Server_ApplyAttackForwardForce_Implementation()
{
	// 이동(Launch)은 서버에서 수행해야 위치 동기화됨
	FVector ForwardDir = GetActorForwardVector();
	FVector LaunchVel = ForwardDir * AttackForwardForce;
	LaunchCharacter(LaunchVel, true, false);
}

// =================================================================
// [스킬 1 (공중 콤보) 구현]
// =================================================================

void AStrikerCharacter::Skill1()
{
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime < Skill1ReadyTime) return;

	// 클라이언트에서 가장 적절한 공중 타겟 찾기
	AActor* Target = FindBestAirborneTarget();
	if (Target)
	{
		// 쿨타임 계산
		float ActualCooldown = 0.0f;
		if (SkillDataTable)
		{
			static const FString ContextString(TEXT("StrikerSkill1Cooldown"));
			FSkillData* Data = SkillDataTable->FindRow<FSkillData>(TEXT("Skill1"), ContextString);
			if (Data && Data->Cooldown > 0.0f) ActualCooldown = Data->Cooldown;
		}
		Skill1ReadyTime = CurrentTime + ActualCooldown;

		// 서버에 스킬 시전 요청
		Server_TrySkill1(Target);
	}
}

void AStrikerCharacter::Server_TrySkill1_Implementation(AActor* TargetActor)
{
	ACharacter* TargetChar = Cast<ACharacter>(TargetActor);
	if (!TargetChar) return;

	// 거리 체크
	float DistSq = FVector::DistSquared(GetActorLocation(), TargetActor->GetActorLocation());
	if (DistSq > 1500.f * 1500.f) return;

	float SkillDamage = 0.f;
	if (SkillDataTable)
	{
		static const FString ContextString(TEXT("Striker Skill1 Context"));
		FSkillData* Data = SkillDataTable->FindRow<FSkillData>(TEXT("Skill1"), ContextString);
		if (Data)
		{
			if (!IsSkillUnlocked(Data->RequiredStage)) return;
			SkillDamage = Data->Damage;
			if (Data->SkillMontage) Multicast_PlaySkill1FX(TargetChar);
		}
	}

	// 1. 순간이동 (서버 권한)
	FVector TeleportLoc = TargetChar->GetActorLocation() - (TargetChar->GetActorForwardVector() * 100.f) + FVector(0, 0, 50.f);
	SetActorLocation(TeleportLoc);

	// 2. 회전 보정
	FVector LookDir = TargetChar->GetActorLocation() - TeleportLoc;
	LookDir.Z = 0.f;
	SetActorRotation(LookDir.Rotation());

	// 3. 타겟 상태 정지 및 데미지
	SuspendTarget(TargetChar);
	UGameplayStatics::ApplyDamage(TargetChar, SkillDamage, GetController(), this, UDamageType::StaticClass());

	// 4. 내 상태 정지 (중력 무시)
	GetCharacterMovement()->GravityScale = 0.f;
	GetCharacterMovement()->Velocity = FVector::ZeroVector;

	CurrentComboTarget = TargetChar;

	// 종료 타이머
	GetWorldTimerManager().SetTimer(Skill1TimerHandle, this, &AStrikerCharacter::EndSkill1, 1.0f, false);
}

void AStrikerCharacter::EndSkill1()
{
	GetCharacterMovement()->GravityScale = 1.0f;
	if (CurrentComboTarget)
	{
		ReleaseTarget(CurrentComboTarget);
		CurrentComboTarget = nullptr;
	}
}

void AStrikerCharacter::Multicast_PlaySkill1FX_Implementation(AActor* Target)
{
	ProcessSkill(TEXT("Skill1"));
}

// =================================================================
// [스킬 2 (내려찍기) 구현]
// =================================================================

void AStrikerCharacter::Skill2()
{
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime < Skill2ReadyTime) return;

	// 공중에서만 사용 가능
	if (GetCharacterMovement()->IsFalling())
	{
		Server_StartSkill2();
	}
}

void AStrikerCharacter::Server_StartSkill2_Implementation()
{
	float CurrentTime = GetWorld()->GetTimeSeconds();
	float ActualCooldown = 0.0f;

	if (SkillDataTable)
	{
		static const FString ContextString(TEXT("StrikerSkill2Cooldown"));
		FSkillData* Data = SkillDataTable->FindRow<FSkillData>(TEXT("Skill2"), ContextString);
		if (Data && Data->Cooldown > 0.0f) ActualCooldown = Data->Cooldown;
	}
	Skill2ReadyTime = CurrentTime + ActualCooldown;

	// 1. 애니메이션 동기화
	ProcessSkill(TEXT("Skill2")); // Multicast or Replicated Montage 필요

	// 2. 물리적 하강 (서버)
	FVector SlamVelocity = FVector(0.f, 0.f, -3000.f);
	LaunchCharacter(SlamVelocity, true, true);

	bIsSlamming = true;
}

void AStrikerCharacter::Server_Skill2Impact_Implementation()
{
	FVector MyLoc = GetActorLocation();
	Multicast_PlaySlamFX();

	// 범위 데미지 및 물리력
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

		// 적군 판별
		bool bIsNormal = Actor->IsA(AEnemyNormal::StaticClass());
		bool bIsBaseChar = Actor->IsA(ABaseCharacter::StaticClass());

		// 데미지 및 끌어오기/밀쳐내기 효과
		if (bIsNormal)
		{
			UGameplayStatics::ApplyDamage(TargetChar, SlamDamage, GetController(), this, UDamageType::StaticClass());
			FVector PullDir = (MyLoc - TargetChar->GetActorLocation()).GetSafeNormal();
			FVector PullVel = (PullDir * 1500.f) + FVector(0.f, 0.f, 300.f);
			TargetChar->LaunchCharacter(PullVel, true, true);
		}
		else if (bIsBaseChar)
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

// =================================================================
// [직업 스킬 (JobAbility) 구현]
// =================================================================

void AStrikerCharacter::JobAbility()
{
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime < JobAbilityCooldownTime) return;

	float ActualCooldown = 5.0f;
	if (SkillDataTable)
	{
		static const FString ContextString(TEXT("StrikerJobCooldown"));
		FSkillData* Data = SkillDataTable->FindRow<FSkillData>(TEXT("JobAbility"), ContextString);
		if (Data && Data->Cooldown > 0.0f) ActualCooldown = Data->Cooldown;
	}

	Server_UseJobAbility();
	JobAbilityCooldownTime = CurrentTime + ActualCooldown;
}

void AStrikerCharacter::Server_UseJobAbility_Implementation()
{
	float AbilityDamage = 50.f;
	ProcessSkill(TEXT("JobAbility")); // 몽타주 재생

	if (SkillDataTable)
	{
		static const FString ContextString(TEXT("Striker JobAbility Damage"));
		FSkillData* Data = SkillDataTable->FindRow<FSkillData>(TEXT("JobAbility"), ContextString);
		if (Data) AbilityDamage = Data->Damage;
	}

	// 박스 범위 내 적 타격
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
		bool bIsBaseChar = Actor->IsA(ABaseCharacter::StaticClass());

		if (bIsNormal)
		{
			UGameplayStatics::ApplyDamage(TargetChar, AbilityDamage, GetController(), this, UDamageType::StaticClass());
			FVector LaunchVel = FVector(0.f, 0.f, 1000.f);
			TargetChar->LaunchCharacter(LaunchVel, true, true);
		}
		else if (bIsBaseChar)
		{
			FVector LaunchVel = FVector(0.f, 0.f, 1000.f);
			TargetChar->LaunchCharacter(LaunchVel, true, true);
		}
	}
}

// =================================================================
// [헬퍼 함수]
// =================================================================

void AStrikerCharacter::ProcessSkill(FName SkillRowName)
{
	if (!SkillDataTable) return;

	static const FString ContextString(TEXT("StrikerSkillContext"));
	FSkillData* Data = SkillDataTable->FindRow<FSkillData>(SkillRowName, ContextString);

	if (Data && Data->SkillMontage)
	{
		if (!IsSkillUnlocked(Data->RequiredStage)) return;
		PlayAnimMontage(Data->SkillMontage);
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

		if (!bIsNormal && !bIsMidBoss) continue;

		if (CharActor->GetCharacterMovement()->IsFalling())
		{
			float HeightDiff = CharActor->GetActorLocation().Z - MyLoc.Z;
			if (HeightDiff < Skill1RequiredHeight) continue;

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