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
// [평타] 겟앰프드 스타일
// =============================================================
void AStrikerCharacter::Attack()
{
	ProcessSkill(TEXT("Attack"));

	FVector ForwardDir = GetActorForwardVector();
	FVector LaunchVel = ForwardDir * AttackForwardForce;
	LaunchCharacter(LaunchVel, true, false);
}

void AStrikerCharacter::Skill1()
{
	// [로그 1] 키 입력 확인
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, TEXT(">> [Skill1] Input Pressed!"));

	AActor* Target = FindBestAirborneTarget();
	if (Target)
	{
		// [로그 2] 타겟 발견 성공
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, FString::Printf(TEXT(">> [Skill1] Client: Found Target [%s]"), *Target->GetName()));
		Server_TrySkill1(Target);
	}
	else
	{
		// [로그 3] 타겟 없음 -> 여기서 뜨면 FindBestAirborneTarget 로직 문제
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT(">> [Skill1] Client: No Airborne Target Found!"));
	}
}

AActor* AStrikerCharacter::FindBestAirborneTarget()
{
	FVector MyLoc = GetActorLocation();
	FVector CamFwd = GetControlRotation().Vector();
	CamFwd.Z = 0.f; // 수평 시야
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

		// 1. 적군 판별 (EnemyNormal 또는 EnemyMidBoss)
		bool bIsNormal = Actor->IsA(AEnemyNormal::StaticClass());
		bool bIsMidBoss = Actor->IsA(AEnemyMidBoss::StaticClass());

		// 적이 아니면(팀원이면) 1번 스킬 대상 아님 -> 패스
		if (!bIsNormal && !bIsMidBoss) continue;

		// 2. 공중 체크
		bool bIsFalling = CharActor->GetCharacterMovement()->IsFalling();
		// (테스트용 강제 true 필요하면 주석 해제)
		// bIsFalling = true; 

		if (bIsFalling)
		{
			// 3. [추가] 높이 체크 (땅에서 얼마나 떴는지)
			// 적의 발밑 높이 - 내 발밑 높이
			float HeightDiff = CharActor->GetActorLocation().Z - MyLoc.Z;

			if (HeightDiff < Skill1RequiredHeight)
			{
				// 너무 낮게 떠 있으면 안 씀
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
	// [로그 7] 서버 도착 확인
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Blue, TEXT(">> [Server] Request Received"));

	ACharacter* TargetChar = Cast<ACharacter>(TargetActor);
	if (!TargetChar) return;

	float DistSq = FVector::DistSquared(GetActorLocation(), TargetActor->GetActorLocation());
	if (DistSq > 1500.f * 1500.f)
	{
		// [로그 8] 거리 너무 멀음
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
			// [로그 9] 데이터 테이블 로드 성공 및 잠금 확인
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
			// [로그 10] 데이터 테이블 로우 없음
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT(">> [Server] 'Skill1' Row Not Found in DataTable!"));
		}
	}
	else
	{
		// [로그 11] 데이터 테이블 자체가 Null
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
// [직업 능력] 전방 직사각형 범위 띄우기
// =============================================================
void AStrikerCharacter::JobAbility()
{
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime < JobAbilityCooldownTime)
	{
		// [수정] FColor::Gray -> FColor::Silver
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Silver, TEXT("Job Ability Cooldown!"));
		return;
	}

	Server_UseJobAbility();
	JobAbilityCooldownTime = CurrentTime + 5.0f;
}

void AStrikerCharacter::Server_UseJobAbility_Implementation()
{
	float AbilityDamage = 50.f;
	ProcessSkill(TEXT("JobAbility"));

	// 데이터 테이블 데미지 가져오기
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
		bool bIsBaseChar = Actor->IsA(ABaseCharacter::StaticClass()); // 팀원 포함

		// 1. EnemyNormal (쫄몹): 데미지 O, 띄우기 O
		if (bIsNormal)
		{
			UGameplayStatics::ApplyDamage(TargetChar, AbilityDamage, GetController(), this, UDamageType::StaticClass());

			FVector LaunchVel = FVector(0.f, 0.f, 1000.f);
			TargetChar->LaunchCharacter(LaunchVel, true, true);
		}
		// 2. Team (팀원): 데미지 X, 띄우기 O (효과 받음)
		else if (bIsBaseChar && !bIsNormal && !bIsMidBoss)
		{
			FVector LaunchVel = FVector(0.f, 0.f, 1000.f);
			TargetChar->LaunchCharacter(LaunchVel, true, true);
		}
		// 3. Boss (보스): 면역 (아무것도 안 함)
		// (보스는 무거워서 안 뜬다는 컨셉)
	}
}

// =============================================================
// [스킬 2] 공중 찍기
// =============================================================
void AStrikerCharacter::Skill2()
{
	if (GetCharacterMovement()->IsFalling())
	{
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

		// 1. EnemyNormal (쫄몹): 데미지 O + 당겨오기
		if (bIsNormal)
		{
			UGameplayStatics::ApplyDamage(TargetChar, SlamDamage, GetController(), this, UDamageType::StaticClass());

			FVector PullDir = (MyLoc - TargetChar->GetActorLocation()).GetSafeNormal();
			FVector PullVel = (PullDir * 1500.f) + FVector(0.f, 0.f, 300.f);
			TargetChar->LaunchCharacter(PullVel, true, true);
		}
		// 2. Team (팀원): 데미지 X + 밀쳐내기 (효과 받음)
		else if (bIsBaseChar && !bIsMidBoss)
		{
			FVector PushDir = (TargetChar->GetActorLocation() - MyLoc).GetSafeNormal();
			FVector PushVel = (PushDir * 800.f) + FVector(0.f, 0.f, 200.f);
			TargetChar->LaunchCharacter(PushVel, true, true);
		}
		// 3. Boss (보스): 면역 (2번 스킬은 쫄몹만)
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
