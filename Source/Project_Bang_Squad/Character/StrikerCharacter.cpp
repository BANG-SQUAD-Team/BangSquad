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

	// 스트라이커 판정 박스 크기
	HitBoxSize = FVector(40.0f, 40.0f, 40.0f);
	
		// 속도 기본값
		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			MoveComp->MaxWalkSpeed = 660.f;
		}
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

    // 타이머 정리 (혹시 켜져있으면)
    GetWorldTimerManager().ClearTimer(GroundCheckTimerHandle);

    if (bIsSlamming)
    {
        bIsSlamming = false;

        if (!bHasTriggeredLandAnim)
        {
            Multicast_Skill2();
        }

        Server_Skill2Impact(); // 데미지 처리
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
    GetWorldTimerManager().ClearTimer(SlashLoopTimerHandle);
	GetWorldTimerManager().ClearTimer(AttackHitTimerHandle);
	GetWorldTimerManager().ClearTimer(HitLoopTimerHandle);

	bIsSlamming = false;
	StopAnimMontage();
	Super::OnDeath();
}

// =================================================================
// [입력 핸들러 및 평타
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
//  스윕(Trace) 판정 로직 구현
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
    if (JobAbilityEffectClass)
    {
        // 머리 위 3미터(300) 정도 위치
        FVector SpawnLocation = GetActorLocation() + FVector(0.0f, 0.0f, 300.0f);
        FRotator SpawnRotation = FRotator::ZeroRotator;

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.Instigator = this;

        // 월드에 액터 소환
        GetWorld()->SpawnActor<AActor>(
            JobAbilityEffectClass,
            SpawnLocation,
            SpawnRotation,
            SpawnParams
        );
    }

    // =================================================================
    // 2. [GameLogic] 데미지 및 물리 처리 (서버만 실행)
    // =================================================================
    // 데미지나 물리력(Launch)은 서버에서만 계산해야 합니다.
    if (!HasAuthority()) return;

    // --- 기존 데미지 로직 시작 ---
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

void AStrikerCharacter::Multicast_PlaySkill1FX_Implementation(AActor* Target)
{
    GetMesh()->SetVisibility(false);

    // 1. 기존 몽타주 재생 (애니메이션)
    ProcessSkill(TEXT("Skill1"));

    // 혹시 켜져 있을지 모를 타이머 끄기 (중복 실행 방지)
    GetWorldTimerManager().ClearTimer(SlashLoopTimerHandle);

    // [수정] 이제 Niagara(Skill1SlashVFX)가 아니라 액터 클래스(SlashActorClass)를 확인해야 합니다.
    if (Target && SlashActorClass)
    {
        // 0.05초마다 액터 소환 함수 실행
        GetWorldTimerManager().SetTimer(SlashLoopTimerHandle, this, &AStrikerCharacter::SpawnRandomSlashFX, 0.05f, true);
    }
}

void AStrikerCharacter::SpawnRandomSlashFX()
{
    // 1. 안전장치: 타겟이 사라졌거나, 베기 이펙트(SlashActorClass)가 없으면 타이머 종료
    if (!CurrentComboTarget || !SlashActorClass)
    {
        GetWorldTimerManager().ClearTimer(SlashLoopTimerHandle);
        return;
    }

    FVector TargetLoc = CurrentComboTarget->GetActorLocation();

    // 2. 한 번에 2세트씩 소환 (베기 + 잔상)
    for (int32 i = 0; i < 2; i++)
    {
        // [위치 계산] 타겟 주변 50~100 거리의 랜덤한 허공
        FVector RandomOffset = FMath::VRand() * FMath::RandRange(50.0f, 100.0f);
        FVector SpawnLoc = TargetLoc + RandomOffset;

        // [공통 옵션] 벽 속에 있어도 무조건 소환해라
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        // ====================================================
        // A. 베기 이펙트 (빨간 막대기) 소환
        // ====================================================
        // 막대기는 방향이 제멋대로여야 멋있음 (완전 랜덤 회전)
        FRotator BeamRot = FRotator(FMath::RandRange(0.f, 360.f), FMath::RandRange(0.f, 360.f), FMath::RandRange(0.f, 360.f));

        AActor* SpawnedSlash = GetWorld()->SpawnActor<AActor>(SlashActorClass, SpawnLoc, BeamRot, SpawnParams);
        if (SpawnedSlash)
        {
            // 크기 조절 안 함 (BP 설정 그대로 나옴)
            SpawnedSlash->SetLifeSpan(0.1f); // 0.1초 뒤 삭제
        }

        // ====================================================
        // B. 유령 잔상 (캐릭터 분신) 소환
        // ====================================================
        if (GhostActorClass)
        {
            // 유령은 타겟을 쳐다보면서 소환되어야 공격하는 것 같음
            FRotator LookAtRot = (TargetLoc - SpawnLoc).Rotation();
            // (만약 유령도 랜덤으로 돌리고 싶으면 위 BeamRot을 넣으세요)

            AActor* SpawnedGhost = GetWorld()->SpawnActor<AActor>(GhostActorClass, SpawnLoc, LookAtRot, SpawnParams);
            if (SpawnedGhost)
            {
                SpawnedGhost->SetLifeSpan(0.1f); // 유령도 0.1초 뒤 삭제 (번쩍!)
            }
        }
    }
}

// [수정] 스킬 종료 처리
void AStrikerCharacter::EndSkill1()
{
    // 1. 난도질 타이머 종료 (더 이상 이펙트 안 나옴)
    GetWorldTimerManager().ClearTimer(SlashLoopTimerHandle);

    GetCharacterMovement()->GravityScale = 1.0f;

    GetWorldTimerManager().ClearTimer(SlashLoopTimerHandle);

    // [추가] 캐릭터를 다시 짠! 하고 나타나게 합니다.
    GetMesh()->SetVisibility(true);

    if (CurrentComboTarget)
    {
        ReleaseTarget(CurrentComboTarget);
        CurrentComboTarget = nullptr;
    }
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
    // 1. 쿨타임 로직 (기존 동일)
    float CurrentTime = GetWorld()->GetTimeSeconds();
    float ActualCooldown = 0.0f;
    if (SkillDataTable)
    {
        static const FString ContextString(TEXT("StrikerSkill2Cooldown"));
        FSkillData* Data = SkillDataTable->FindRow<FSkillData>(TEXT("Skill2"), ContextString);
        if (Data && Data->Cooldown > 0.0f) ActualCooldown = Data->Cooldown;
    }
    Skill2ReadyTime = CurrentTime + ActualCooldown;

    // 2. [중요] 여기서 Multicast_Skill2()를 부르지 않습니다! (아직 구르면 안됨)
    // 대신 그냥 떨어지는 힘만 가합니다.
    bIsSlamming = true;
    bHasTriggeredLandAnim = false; // 애니메이션 플래그 초기화

    // 3. 강력하게 하강
    FVector SlamVelocity = FVector(0.f, 0.f, -3500.f);
    LaunchCharacter(SlamVelocity, true, true);

    // 4. [추가] 0.02초마다 바닥과의 거리를 체크 시작
    GetWorldTimerManager().SetTimer(GroundCheckTimerHandle, this, &AStrikerCharacter::CheckGroundDistanceForSkill2, 0.02f, true);
}

// 이 함수가 떨어지는 내내 실행되면서 바닥을 감시합니다.
void AStrikerCharacter::CheckGroundDistanceForSkill2()
{
    if (!bIsSlamming)
    {
        // 스킬이 끝났으면 타이머 종료
        GetWorldTimerManager().ClearTimer(GroundCheckTimerHandle);
        return;
    }

    FVector Start = GetActorLocation();
    FVector End = Start + (FVector::DownVector * 3000.0f); // 내 발밑 600 unit (약 6미터) 감지

    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    // 바닥으로 레이저 발사
    bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        Start,
        End,
        ECC_Visibility, // 혹은 ECC_WorldStatic
        Params
    );

    // 바닥이 감지되었고 + 아직 애니메이션을 안 틀었다면
    if (bHit && !bHasTriggeredLandAnim)
    {
        bHasTriggeredLandAnim = true;

        // [핵심] 땅에 닿기 직전! 이제 구르기 몽타주를 틉니다.
        // 몽타주의 앞부분(준비 동작)이 재생되면서 땅에 닿게 됩니다.
        Multicast_Skill2();

        // 더 이상 체크할 필요 없으니 타이머 끔
        GetWorldTimerManager().ClearTimer(GroundCheckTimerHandle);
    }
}

void AStrikerCharacter::Server_Skill2Impact_Implementation()
{
    FVector MyLoc = GetActorLocation();
    Multicast_PlaySlamFX();

    TArray<AActor*> OverlappingActors;
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

    // [변경] 공격 반경: 600.f -> 300.f (지름 6m, 일반적인 광역기 범위)
    float AttackRadius = 300.0f;

    UKismetSystemLibrary::SphereOverlapActors(GetWorld(), MyLoc, AttackRadius, ObjectTypes, ACharacter::StaticClass(), { this }, OverlappingActors);

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

// [변경] 이펙트 재생 함수
void AStrikerCharacter::Multicast_PlaySlamFX_Implementation()
{
    // [기존 디버그 메시지]
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Purple, TEXT("SLAM IMPACT!!"));

    // 1. 데칼 생성
    if (Skill2DecalMaterial)
    {
        FVector SpawnLocation = GetActorLocation();
        SpawnLocation.Z -= 20.0f;

        FRotator DecalRotation = FRotator(-90.0f, 0.0f, 0.0f);
        DecalRotation.Roll = FMath::RandRange(0.0f, 360.0f);

        UGameplayStatics::SpawnDecalAtLocation(
            GetWorld(),
            Skill2DecalMaterial,
            Skill2DecalSize, // 헤더에서 200으로 설정한 값 사용
            SpawnLocation,
            DecalRotation,
            Skill2DecalLifeSpan
        );
    }

    // 2. 카메라 흔들림 (범위 조정)
    if (Skill2CameraShakeClass)
    {
        UGameplayStatics::PlayWorldCameraShake(
            GetWorld(),
            Skill2CameraShakeClass,
            GetActorLocation(),
            300.0f,   // [변경] Inner Radius: 공격 범위(300) 안에서는 최대 강도
            1500.0f,  // [변경] Outer Radius: 15미터 밖에서는 흔들림 없음 (기존 2000보다 줄임)
            1.0f,
            false
        );
    }
}

void AStrikerCharacter::Multicast_Skill2_Implementation()
{
    if (!SkillDataTable) return;

    static const FString ContextString(TEXT("Skill2 Montage Skip"));
    FSkillData* Data = SkillDataTable->FindRow<FSkillData>(TEXT("Skill2"), ContextString);

    if (Data && Data->SkillMontage)
    {
        UAnimInstance* AnimInstance = (GetMesh()) ? GetMesh()->GetAnimInstance() : nullptr;
        if (AnimInstance)
        {
            // 1. 몽타주 재생
            float PlayDuration = AnimInstance->Montage_Play(Data->SkillMontage, 1.0f);

            // 2. [핵심] 0.2초나 0.3초 뒤로 강제 점프 (앞부분 스킵)
            // 이 수치를 늘릴수록 구르기 동작이 더 빨리 나옵니다.
            float SkipTime = 0.2f;

            // 몽타주가 시작되자마자 해당 시간 위치로 건너뜁니다.
            if (PlayDuration > 0.f)
            {
                AnimInstance->Montage_SetPosition(Data->SkillMontage, SkipTime);
            }
        }
    }
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
    // 1. 스킬 로직 (기존 유지)
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