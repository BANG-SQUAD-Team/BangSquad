#include "Project_Bang_Squad/Character/StrikerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"
#include "Project_Bang_Squad/Character/Enemy/EnemyNormal.h"
#include "Project_Bang_Squad/Character/Enemy/EnemyMidBoss.h"

// [주의] BoxComponent 삭제됨

AStrikerCharacter::AStrikerCharacter()
{
	bIsSlamming = false;

	// 스트라이커 판정 박스 크기
	HitBoxSize = FVector(40.0f, 40.0f, 40.0f);
	
	// 속도 기본값
	GetCharacterMovement()->MaxWalkSpeed = 660.f;
}

void AStrikerCharacter::BeginPlay()
{
	Super::BeginPlay();
	

    if (GetMesh())
    {
        GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    }
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

void AStrikerCharacter::OnDeath()
{
	if (bIsDead) return;
	if (CurrentComboTarget)
	{
		ReleaseTarget(CurrentComboTarget);
		CurrentComboTarget = nullptr;
	}
	GetWorldTimerManager().ClearTimer(Skill1TimerHandle);

	// [추가] 평타 판정 타이머 정리
	GetWorldTimerManager().ClearTimer(AttackHitTimerHandle);
	GetWorldTimerManager().ClearTimer(HitLoopTimerHandle);

	bIsSlamming = false;
	StopAnimMontage();
	Super::OnDeath();
}

// =================================================================
// [입력 핸들러 및 평타 (팔라딘 스타일)]
// =================================================================

void AStrikerCharacter::Attack()
{
    if (!CanAttack()) return;

    FName SkillRowName = bIsNextAttackA ? TEXT("Attack_A") : TEXT("Attack_B");

    if (SkillDataTable)
    {
        static const FString ContextString(TEXT("StrikerAttack_Local"));
        FSkillData* Row = SkillDataTable->FindRow<FSkillData>(SkillRowName, ContextString);
        if (Row)
        {
            // AttackCooldownTime은 BaseCharacter에 있다고 가정 (StartAttackCooldown에서 사용됨)
            if (Row->Cooldown > 0.0f)
            {
                AttackCooldownTime = Row->Cooldown;
            }
        }
    }
    Server_Attack(SkillRowName);
    bIsNextAttackA = !bIsNextAttackA;
	// 로컬에서 쿨타임 시작 (다음 클릭 방지)
	StartAttackCooldown();
}

void AStrikerCharacter::Server_Attack_Implementation(FName SkillName)
{
    if (SkillName == TEXT("Attack_A"))
    {
        MyAttackSocket = TEXT("Hand_R_Socket"); // A 공격 -> 오른손
    }
    else
    {
        MyAttackSocket = TEXT("Hand_L_Socket"); // B 공격 -> 왼손
    }

	float ActionDelay = 0.0f; // 기본 딜레이

	if (SkillDataTable)
	{
		static const FString ContextString(TEXT("StrikerAttack"));
		FSkillData* Row = SkillDataTable->FindRow<FSkillData>(SkillName, ContextString);

		if (Row)
		{
			CurrentSkillDamage = Row->Damage;
			if (Row->Cooldown > 0.0f) AttackCooldownTime = Row->Cooldown;

			// 데이터 테이블에서 딜레이 시간 가져옴
			ActionDelay = Row->ActionDelay;
		}
	}
	
	Multicast_Attack(SkillName);

	// 타이머 초기화
	GetWorldTimerManager().ClearTimer(AttackHitTimerHandle);
	GetWorldTimerManager().ClearTimer(HitLoopTimerHandle);

	if (ActionDelay > 0.0f)
	{
		// 딜레이 후 판정 시작
		GetWorldTimerManager().SetTimer(AttackHitTimerHandle, this, &AStrikerCharacter::StartMeleeTrace, ActionDelay, false);
	}
	else
	{
		// 즉시 판정 시작
		StartMeleeTrace();
	}
	StartAttackCooldown();
}

void AStrikerCharacter::Multicast_Attack_Implementation(FName SkillName)
{
	ProcessSkill(SkillName);
}

// =========================================================
// [팔라딘 스타일] 스윕(Trace) 판정 로직 구현
// =========================================================

void AStrikerCharacter::StartMeleeTrace()
{
    SwingDamagedActors.Empty();

    // "Hand_R_Socket" 대신 -> MyAttackSocket 사용
    if (GetMesh() && GetMesh()->DoesSocketExist(MyAttackSocket))
    {
        LastHandLocation = GetMesh()->GetSocketLocation(MyAttackSocket);
    }
    else
    {
        LastHandLocation = GetActorLocation() + GetActorForwardVector() * 100.f;
    }

	GetWorldTimerManager().SetTimer(HitLoopTimerHandle, this, &AStrikerCharacter::PerformMeleeTrace, 0.015f, true);

	FTimerHandle StopTimer;
	GetWorldTimerManager().SetTimer(StopTimer, this, &AStrikerCharacter::StopMeleeTrace, HitDuration, false);
}

void AStrikerCharacter::PerformMeleeTrace()
{
	FVector CurrentLoc;
	FQuat CurrentRot;

    if (GetMesh() && GetMesh()->DoesSocketExist(MyAttackSocket))
    {
        CurrentLoc = GetMesh()->GetSocketLocation(MyAttackSocket);
        CurrentRot = GetMesh()->GetSocketQuaternion(MyAttackSocket);
    }
	else
	{
		CurrentLoc = GetActorLocation() + GetActorForwardVector() * 100.f;
		CurrentRot = GetActorQuat();
	}

	TArray<FHitResult> HitResults;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	bool bHit = GetWorld()->SweepMultiByChannel(
		HitResults,
		LastHandLocation,
		CurrentLoc,
		CurrentRot,
		ECC_Pawn,
		FCollisionShape::MakeBox(HitBoxSize),
		Params
	);

	if (bHit)
	{
		for (const FHitResult& Hit : HitResults)
		{
			AActor* HitActor = Hit.GetActor();

			if (HitActor && HitActor != this && !SwingDamagedActors.Contains(HitActor))
			{
				// [팀킬 방지] Player 태그 체크
				if (HitActor->ActorHasTag("Player")) continue;

				float Dmg = (CurrentSkillDamage > 0.0f) ? CurrentSkillDamage : 10.0f;
				UGameplayStatics::ApplyDamage(
					HitActor,
					Dmg,
					GetController(),
					this,
					UDamageType::StaticClass()
				);

				SwingDamagedActors.Add(HitActor);
			}
		}
	}

	LastHandLocation = CurrentLoc;
}

void AStrikerCharacter::StopMeleeTrace()
{
	GetWorldTimerManager().ClearTimer(HitLoopTimerHandle);
	SwingDamagedActors.Empty();
}

void AStrikerCharacter::ApplyAttackForwardForce()
{
	if (HasAuthority()) Server_ApplyAttackForwardForce_Implementation();
	else Server_ApplyAttackForwardForce();
}

void AStrikerCharacter::ApplyJobAbilityHit()
{
    // 1. 서버인지 확인 (서버에서만 동작해야 함)
    if (!HasAuthority()) return;

    // [디버그] 서버에서 함수가 불렸는지 로그로 확인
    // UE_LOG(LogTemp, Warning, TEXT("Server: ApplyJobAbilityHit Called!"));

    float AbilityDamage = 50.f;

    if (SkillDataTable)
    {
        static const FString ContextString(TEXT("Striker JobAbility Damage"));
        FSkillData* Data = SkillDataTable->FindRow<FSkillData>(TEXT("JobAbility"), ContextString);
        if (Data) AbilityDamage = Data->Damage;
    }

    FVector MyLoc = GetActorLocation();
    FVector MyFwd = GetActorForwardVector();

    // 판정 박스 위치 및 크기
    FVector BoxCenter = MyLoc + (MyFwd * 150.f);
    FVector BoxExtent = FVector(150.f, 150.f, 150.f); // Y, Z축 범위를 좀 더 넉넉하게 수정

    // [디버그] 눈에 보이는 빨간 박스 그리기 (2초간 유지)
    DrawDebugBox(GetWorld(), BoxCenter, BoxExtent, FColor::Red, false, 2.0f);

    TArray<AActor*> OverlappingActors;
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic)); // [추가] 적이 Pawn이 아닐 경우 대비

    UKismetSystemLibrary::BoxOverlapActors(GetWorld(), BoxCenter, BoxExtent, ObjectTypes, ACharacter::StaticClass(), { this }, OverlappingActors);

    for (AActor* Actor : OverlappingActors)
    {
        ACharacter* TargetChar = Cast<ACharacter>(Actor);
        if (!TargetChar) continue;

        // 아군(Player 태그) 제외 로직이 필요하면 추가
        if (TargetChar->ActorHasTag("Player")) continue;

        bool bIsNormal = Actor->IsA(AEnemyNormal::StaticClass());
        bool bIsBaseChar = Actor->IsA(ABaseCharacter::StaticClass());

        if (bIsNormal || bIsBaseChar)
        {
            // 데미지 적용
            UGameplayStatics::ApplyDamage(TargetChar, AbilityDamage, GetController(), this, UDamageType::StaticClass());

            // [중요 수정 2] 확실하게 띄우기 위한 로직 강화
            // 땅에 붙어있으면 마찰력 때문에 Launch가 약할 수 있으므로 강제로 Falling 상태로 변경
            if (TargetChar->GetCharacterMovement())
            {
                TargetChar->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
            }

            // Z축 1000이면 꽤 높이 뜹니다. XY축 반동이 없으면 제자리 점프처럼 보일 수 있으니 살짝 밀어줍니다.
            FVector LaunchVel = (GetActorForwardVector() * 100.f) + FVector(0.f, 0.f, 1000.f);

            // XY, Z 모두 override(true)
            TargetChar->LaunchCharacter(LaunchVel, true, true);
        }
    }
}
void AStrikerCharacter::Server_ApplyAttackForwardForce_Implementation()
{
	FVector ForwardDir = GetActorForwardVector();
	FVector LaunchVel = ForwardDir * AttackForwardForce;
	LaunchCharacter(LaunchVel, true, false);
}

// 스킬 1
void AStrikerCharacter::Skill1()
{
	if (!CanAttack()) return;
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime < Skill1ReadyTime) return;
	AActor* Target = FindBestAirborneTarget();
	if (Target)
	{
		float ActualCooldown = 0.0f;
		if (SkillDataTable)
		{
			static const FString ContextString(TEXT("StrikerSkill1Cooldown"));
			FSkillData* Data = SkillDataTable->FindRow<FSkillData>(TEXT("Skill1"), ContextString);
			if (Data && Data->Cooldown > 0.0f) ActualCooldown = Data->Cooldown;
		}
		Skill1ReadyTime = CurrentTime + ActualCooldown;
		Server_TrySkill1(Target);
	}
}
void AStrikerCharacter::Server_TrySkill1_Implementation(AActor* TargetActor)
{
	ACharacter* TargetChar = Cast<ACharacter>(TargetActor);
	if (!TargetChar) return;
	float DistSq = FVector::DistSquared(GetActorLocation(), TargetActor->GetActorLocation());
	if (DistSq > 1500.f * 1500.f) return;
	float SkillDamage = 0.f;
	if (SkillDataTable)
	{
		static const FString ContextString(TEXT("Striker Skill1 Context"));
		FSkillData* Data = SkillDataTable->FindRow<FSkillData>(TEXT("Skill1"), ContextString);
		if (Data) { if (!IsSkillUnlocked(Data->RequiredStage)) return; SkillDamage = Data->Damage; if (Data->SkillMontage) Multicast_PlaySkill1FX(TargetChar); }
	}
	FVector TeleportLoc = TargetChar->GetActorLocation() - (TargetChar->GetActorForwardVector() * 100.f) + FVector(0, 0, 50.f);
	SetActorLocation(TeleportLoc);
	FVector LookDir = TargetChar->GetActorLocation() - TeleportLoc; LookDir.Z = 0.f; SetActorRotation(LookDir.Rotation());
	SuspendTarget(TargetChar);
	UGameplayStatics::ApplyDamage(TargetChar, SkillDamage, GetController(), this, UDamageType::StaticClass());
	GetCharacterMovement()->GravityScale = 0.f; GetCharacterMovement()->Velocity = FVector::ZeroVector;
	CurrentComboTarget = TargetChar;
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

// ============================================================================
// 스킬 2 (공중에서 내려찍기)
// ============================================================================
void AStrikerCharacter::Skill2()
{
	if (!CanAttack()) return;
    float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime < Skill2ReadyTime) return;

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

    Multicast_Skill2();

    FVector SlamVelocity = FVector(0.f, 0.f, -3000.f);
    LaunchCharacter(SlamVelocity, true, true);
    bIsSlamming = true;
}

void AStrikerCharacter::Server_Skill2Impact_Implementation()
{
    FVector MyLoc = GetActorLocation();
    Multicast_PlaySlamFX();

    TArray<AActor*> OverlappingActors;
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

    UKismetSystemLibrary::SphereOverlapActors(GetWorld(), MyLoc, 600.f, ObjectTypes, ACharacter::StaticClass(), { this }, OverlappingActors);

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
        bool bIsBaseChar = Actor->IsA(ABaseCharacter::StaticClass());

        if (bIsNormal)
        {
            UGameplayStatics::ApplyDamage(TargetChar, SlamDamage, GetController(), this, UDamageType::StaticClass());

            // 적을 내쪽으로 끌어당기면서 살짝 띄움
            FVector PullDir = (MyLoc - TargetChar->GetActorLocation()).GetSafeNormal();
            FVector PullVel = (PullDir * 1500.f) + FVector(0.f, 0.f, 300.f);
            TargetChar->LaunchCharacter(PullVel, true, true);
        }
        else if (bIsBaseChar)
        {
            // 다른 캐릭터는 밀쳐냄
            FVector PushDir = (TargetChar->GetActorLocation() - MyLoc).GetSafeNormal();
            FVector PushVel = (PushDir * 800.f) + FVector(0.f, 0.f, 200.f);
            TargetChar->LaunchCharacter(PushVel, true, true);
        }
    }
}

void AStrikerCharacter::Multicast_Skill2_Implementation()
{
	ProcessSkill(TEXT("Skill2"));
}

void AStrikerCharacter::Multicast_PlaySlamFX_Implementation()
{
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Purple, TEXT("SLAM IMPACT!!"));
}

// ============================================================================
// 직업 스킬 (Job Ability - 범위 내 적 띄우기)
// ============================================================================
void AStrikerCharacter::JobAbility()
{
	if (!IsSkillUnlocked(1)) return;
	if (!CanAttack()) return;
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
    // 몽타주 재생 명령만 내림 (노티파이가 타점 잡음)
    Multicast_JobAbility();
}
void AStrikerCharacter::Multicast_JobAbility_Implementation()
{
	ProcessSkill(TEXT("JobAbility"));
}

// ============================================================================
// Helper Functions (타겟팅 및 상태 제어)
// ============================================================================
void AStrikerCharacter::ProcessSkill(FName SkillRowName)
{
    if (!SkillDataTable) return;

    static const FString ContextString(TEXT("StrikerSkillContext"));
    FSkillData* Data = SkillDataTable->FindRow<FSkillData>(SkillRowName, ContextString);

    if (Data && Data->SkillMontage)
    {
        if (!IsSkillUnlocked(Data->RequiredStage)) return;
        PlayActionMontage(Data->SkillMontage);
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

    UKismetSystemLibrary::SphereOverlapActors(GetWorld(), MyLoc, 1200.f, ObjectTypes, ACharacter::StaticClass(), { this }, OverlappingActors);

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