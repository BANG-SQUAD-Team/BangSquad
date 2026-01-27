#include "Project_Bang_Squad/Character/TitanCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h" 
#include "TimerManager.h"
#include "Engine/DataTable.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/SpringArmComponent.h"
#include "Project_Bang_Squad/Character/Enemy/EnemyNormal.h"
#include "Project_Bang_Squad/Character/Player/Titan/TitanRock.h"
#include "Project_Bang_Squad/Character/Player/Titan/TitanThrowableActor.h"
#include "Enemy/EnemyMidBoss.h"
#include "StageBoss/StageBossBase.h"

ATitanCharacter::ATitanCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	GetCharacterMovement()->MaxWalkSpeed = 495.f;
	
	ThrowSpawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("ThrowSpawnPoint"));

	if (Camera)
	{
		ThrowSpawnPoint->SetupAttachment(Camera);
	}
	else
	{
		ThrowSpawnPoint->SetupAttachment(GetRootComponent());
	}

	ThrowSpawnPoint->ArrowSize = 0.5f;
	ThrowSpawnPoint->ArrowColor = FColor::Cyan;

	// 타이탄 판정 박스 크기
	HitBoxSize = FVector(80.0f, 80.0f, 80.0f);
	
	// 스플라인 컴포넌트 생성
	TrajectorySpline = CreateDefaultSubobject<USplineComponent>(TEXT("TrajectorySpline"));
	TrajectorySpline->SetupAttachment(RootComponent);
    
	// 초기에는 아무것도 안 보이게 설정
	TrajectorySpline->SetVisibility(false);
}

void ATitanCharacter::BeginPlay()
{
	Super::BeginPlay();

	GetCapsuleComponent()->OnComponentBeginOverlap.AddDynamic(this, &ATitanCharacter::OnChargeOverlap);
	GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &ATitanCharacter::OnChargeHit);
	
	if (SpringArm) 
	{
		DefaultSocketOffset = SpringArm->SocketOffset;
	}
}

void ATitanCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IsLocallyControlled())
	{
		if (SpringArm)
		{
			// 1. 목표 위치 결정 (잡고 있으면 -> AimingOffset, 아니면 -> DefaultOffset)
			FVector TargetOffset = bIsGrabbing ? AimingSocketOffset : DefaultSocketOffset;

			// 2. 현재 위치 가져오기
			FVector CurrentOffset = SpringArm->SocketOffset;

			// 3. 부드럽게 이동
			FVector NewOffset = FMath::VInterpTo(CurrentOffset, TargetOffset, DeltaTime, CameraInterpSpeed);

			// 4. 적용
			SpringArm->SocketOffset = NewOffset;
		}
		
		// =================================================================
		//  잡은 대상이 사라졌는데, 잡고 있는 상태로 남아있는 경우 복구
		// =================================================================
		if (bIsGrabbing && !GrabbedActor)
		{
			bIsGrabbing = false;
			ShowTrajectory(false);
		}

		if (!bIsGrabbing) 
		{
			UpdateHoverHighlight();
			ShowTrajectory(false);
		}
		else
		{
			ShowTrajectory(true);
			UpdateTrajectory();
		}
	}
}

void ATitanCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATitanCharacter, GrabbedActor);
}

void ATitanCharacter::OnDeath()
{
	if (bIsDead) return;

	if (HasAuthority() && bIsGrabbing && GrabbedActor)
	{
		AutoThrowTimeout();
	}

	if (bIsCharging)
	{
		StopCharge();
	}

	if (HoveredActor)
	{
		SetHighlight(HoveredActor, false);
		HoveredActor = nullptr;
	}

	GetWorldTimerManager().ClearTimer(GrabTimerHandle);
	GetWorldTimerManager().ClearTimer(ChargeTimerHandle);
	GetWorldTimerManager().ClearTimer(CooldownTimerHandle);
	GetWorldTimerManager().ClearTimer(Skill2CooldownTimerHandle);
	GetWorldTimerManager().ClearTimer(Skill1CooldownTimerHandle);
	GetWorldTimerManager().ClearTimer(RockThrowTimerHandle);

	//  공격 판정 타이머 정리
	GetWorldTimerManager().ClearTimer(AttackHitTimerHandle);
	GetWorldTimerManager().ClearTimer(HitLoopTimerHandle);

	// 1. 잡기 상태 초기화
	bIsGrabbing = false;
	GrabbedActor = nullptr;
	ShowTrajectory(false); // 궤적도 끄기

	// 2. 쿨타임 플래그 초기화
	bIsCooldown = false;          // 잡기 쿨타임
	bIsSkill1Cooldown = false;    // 스킬1 쿨타임
	bIsSkill2Cooldown = false;    // 스킬2 쿨타임
	bIsCharging = false;          // 돌진 상태

	// 3. 타이머 싹 다 끄기
	GetWorldTimerManager().ClearAllTimersForObject(this);

	// 4. 공격 판정 데이터 초기화
	SwingDamagedActors.Empty();
	HitVictims.Empty();

	StopAnimMontage();
	Super::OnDeath();
	
}

// =================================================================
// [입력 핸들러]
// =================================================================

void ATitanCharacter::Attack()
{
	if (bIsDead) return;
	if (!CanAttack()) return;

	FName SkillRowName = bIsNextAttackA ? TEXT("Attack_A") : TEXT("Attack_B");

	if (SkillDataTable)
	{
		static const FString ContextString(TEXT("TitanAttack_Local"));
		FSkillData* Row = SkillDataTable->FindRow<FSkillData>(SkillRowName, ContextString);
		if (Row && Row->Cooldown > 0.0f)
		{
			AttackCooldownTime = Row->Cooldown;
		}
	}
	StartAttackCooldown(); // 로컬 쿨타임 시작

	Server_Attack(SkillRowName);
	bIsNextAttackA = !bIsNextAttackA;
}

// =================================================================
// [네트워크 구현: 평타 (팔라딘 스타일)]
// =================================================================

void ATitanCharacter::Server_Attack_Implementation(FName SkillName)
{
	if (SkillName == TEXT("Attack_A"))
	{
		MyAttackSocket = TEXT("Hand_R_Socket"); // A 공격 -> 오른손
	}
	else
	{
		MyAttackSocket = TEXT("Hand_L_Socket"); // B 공격 -> 왼손
	}

	// ... (이 아래는 기존 코드 그대로 두세요) ...
	float ActionDelay = 0.0f;

	if (SkillDataTable)
	{
		static const FString ContextString(TEXT("TitanAttack"));
		FSkillData* Row = SkillDataTable->FindRow<FSkillData>(SkillName, ContextString);

		if (Row)
		{
			CurrentSkillDamage = Row->Damage;
			if (Row->Cooldown > 0.0f) AttackCooldownTime = Row->Cooldown;

			// 데이터 테이블에서 딜레이 시간 가져옴
			ActionDelay = Row->ActionDelay;
		}
	}

	StartAttackCooldown();
	Multicast_Attack(SkillName);

	// 타이머 초기화
	GetWorldTimerManager().ClearTimer(AttackHitTimerHandle);
	GetWorldTimerManager().ClearTimer(HitLoopTimerHandle);

	if (ActionDelay > 0.0f)
	{
		// 딜레이 후 판정 시작
		GetWorldTimerManager().SetTimer(AttackHitTimerHandle, this, &ATitanCharacter::StartMeleeTrace, ActionDelay, false);
	}
	else
	{
		// 즉시 판정 시작
		StartMeleeTrace();
	}
}

void ATitanCharacter::Multicast_Attack_Implementation(FName SkillName)
{
	ProcessSkill(SkillName);
}

// =========================================================
// [팔라딘 스타일] 스윕(Trace) 판정 로직 구현
// =========================================================

void ATitanCharacter::StartMeleeTrace()
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

	// 0.015초마다 궤적 검사 시작 (반복)
	GetWorldTimerManager().SetTimer(HitLoopTimerHandle, this, &ATitanCharacter::PerformMeleeTrace, 0.015f, true);

	// 일정 시간(HitDuration) 뒤에 검사 종료 예약
	FTimerHandle StopTimer;
	GetWorldTimerManager().SetTimer(StopTimer, this, &ATitanCharacter::StopMeleeTrace, HitDuration, false);
}

void ATitanCharacter::PerformMeleeTrace()
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
		CurrentLoc = GetActorLocation() + GetActorForwardVector() * 200.f;
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

			// 1. 유효성 검사 & 중복 타격 방지
			if (HitActor && HitActor != this && !SwingDamagedActors.Contains(HitActor))
			{
				// 2. [팀킬 방지] 상대가 Player 태그를 가지고 있으면 때리지 않음
				if (HitActor->ActorHasTag("Player")) continue;

				// 3. 데미지 적용
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

void ATitanCharacter::StopMeleeTrace()
{
	GetWorldTimerManager().ClearTimer(HitLoopTimerHandle);
	SwingDamagedActors.Empty();
}

void ATitanCharacter::Multicast_FixMesh_Implementation(ACharacter* Victim)
{
	if (!Victim || !Victim->IsValidLowLevel()) return;

	// 1. [가장 중요] 클라이언트에서 꺼져있던 콜리전과 이동 기능을 강제로 켭니다.
	// 이걸 안 하면 클라이언트 화면에서 캐릭터가 굳거나 땅을 뚫습니다.
	if (Victim->GetCapsuleComponent())
	{
		Victim->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	if (Victim->GetCharacterMovement())
	{
		// 멈춰있던 무브먼트를 깨우고 Falling 모드로 전환
		Victim->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		Victim->GetCharacterMovement()->GravityScale = 1.0f;
		Victim->GetCharacterMovement()->Velocity = FVector::ZeroVector; // 초기화
	}

	// 2. 캡슐 회전 초기화
	FRotator CurrentRot = Victim->GetActorRotation();
	Victim->SetActorRotation(FRotator(0.f, CurrentRot.Yaw, 0.f));

	// 3. 메쉬 위치/회전 바로잡기 (기존 기능)
	if (Victim->GetMesh())
	{
		Victim->GetMesh()->SetRelativeLocationAndRotation(
			FVector(0.f, 0.f, -90.f),
			FRotator(0.f, -90.f, 0.f)
		);
	}

	// 4. 강제 업데이트 요청
	Victim->ForceNetUpdate();
}

void ATitanCharacter::JobAbility()
{
	
	if (!IsSkillUnlocked(1)) return;
	// 로컬 체크
	if (bIsDead || bIsCooldown) return;

	if (bIsGrabbing)
	{
		if (bIsCooldown) return;
		bIsCooldown = true;
		// 데이터 테이블에서 쿨타임 가져오기
		float LocalThrowCooldown = 3.0f; // 기본값
		if (SkillDataTable)
		{
			FSkillData* Row = SkillDataTable->FindRow<FSkillData>(TEXT("JobAbility"), TEXT("TitanJob_Local"));
			if (Row && Row->Cooldown > 0.0f) LocalThrowCooldown = Row->Cooldown;
		}
		
		GetWorldTimerManager().SetTimer(CooldownTimerHandle, this, &ATitanCharacter::ResetCooldown, LocalThrowCooldown, false);
		FVector ThrowOrigin = ThrowSpawnPoint->GetComponentLocation();
		Server_ThrowTarget(ThrowOrigin);
	}
	else
	{
		if (!CanAttack()) return;
		if (bIsCooldown) return;
		if (HoveredActor) Server_TryGrab(HoveredActor);
	}
}

void ATitanCharacter::Skill1()
{
	if (!CanAttack()) return;
	if (bIsDead || bIsSkill1Cooldown) return;

	// [수정] 클라이언트 선제적 쿨타임 적용
	bIsSkill1Cooldown = true;

	float LocalCooldown = 3.0f; // 기본값
	if (SkillDataTable)
	{
		static const FString ContextString(TEXT("TitanSkill1_Local"));
		FSkillData* Row = SkillDataTable->FindRow<FSkillData>(TEXT("Skill1"), ContextString);
		if (Row && Row->Cooldown > 0.0f) LocalCooldown = Row->Cooldown;
	}

	GetWorldTimerManager().SetTimer(Skill1CooldownTimerHandle, this, &ATitanCharacter::ResetSkill1Cooldown, LocalCooldown, false);

	Server_Skill1();
}

void ATitanCharacter::Skill2()
{
	if (!CanAttack()) return;
	if (bIsDead || bIsSkill2Cooldown) return;

	// [수정] 클라이언트 선제적 쿨타임 적용
	bIsSkill2Cooldown = true;

	float LocalCooldown = 5.0f; // 기본값
	if (SkillDataTable)
	{
		static const FString ContextString(TEXT("TitanSkill2_Local"));
		FSkillData* Row = SkillDataTable->FindRow<FSkillData>(TEXT("Skill2"), ContextString);
		if (Row && Row->Cooldown > 0.0f) LocalCooldown = Row->Cooldown;
	}

	GetWorldTimerManager().SetTimer(Skill2CooldownTimerHandle, this, &ATitanCharacter::ResetSkill2Cooldown, LocalCooldown, false);

	Server_Skill2();
}
void ATitanCharacter::ExecuteGrab() { if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Magenta, TEXT("ExecuteGrab Called")); }

void ATitanCharacter::Server_TryGrab_Implementation(AActor* TargetToGrab)
{
	if (!TargetToGrab || bIsGrabbing) return;

	if (TargetToGrab->IsA(AEnemyMidBoss::StaticClass()) ||
		TargetToGrab->IsA(AStageBossBase::StaticClass()) ||
		TargetToGrab->ActorHasTag("Boss") ||
		TargetToGrab->ActorHasTag("MidBoss"))
	{
		return;
	}

	float DistSq = FVector::DistSquared(GetActorLocation(), TargetToGrab->GetActorLocation());
	if (DistSq > 1000.f * 1000.f) return;

	GrabbedActor = TargetToGrab;
	bIsGrabbing = true;

	// [수정] 사물일 경우: 물리와 충돌을 모두 꺼야 손에 정확히 붙습니다.
	if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(GrabbedActor->GetRootComponent()))
	{
		RootComp->SetSimulatePhysics(false);
		RootComp->SetCollisionProfileName(TEXT("NoCollision")); // ★ 이거 필수
	}

	GrabbedActor->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Hand_R_Socket"));

	// 캐릭터인 경우 추가 처리
	if (ACharacter* Victim = Cast<ACharacter>(GrabbedActor))
	{
		SetHeldState(Victim, true);
	}

	Multicast_PlayJobMontage(TEXT("Grab"));
	GetWorldTimerManager().SetTimer(GrabTimerHandle, this, &ATitanCharacter::AutoThrowTimeout, GrabMaxDuration, false);
}

void ATitanCharacter::AutoThrowTimeout()
{ 
	FVector Forward = GetActorForwardVector(); 
	FVector ThrowOrigin = GetActorLocation() + FVector(0.f, 0.f, 60.f) + (Forward * 200.0f);
	Server_ThrowTarget(ThrowOrigin); 
}

void ATitanCharacter::OnRep_GrabbedActor()
{
	if (GrabbedActor)
	{
		// [잡았을 때: 기존 로직 유지]
		bIsGrabbing = true;

		if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(GrabbedActor->GetRootComponent()))
		{
			RootComp->SetSimulatePhysics(false);
			RootComp->SetCollisionProfileName(TEXT("NoCollision"));
		}

		// 손에 딱 붙이기
		GrabbedActor->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Hand_R_Socket"));

		if (ACharacter* Victim = Cast<ACharacter>(GrabbedActor))
		{
			SetHeldState(Victim, true);
		}
	}
	else
	{
		// [놓았을 때: 여기가 핵심입니다]
		bIsGrabbing = false;

		// 내 손(Hand_R_Socket)에 아직 붙어있는 녀석들을 찾아서 강제로 떼어냅니다.
		TArray<AActor*> AttachedActors;
		GetAttachedActors(AttachedActors);

		for (AActor* Child : AttachedActors)
		{
			// 진짜 내 손에 붙은 놈인지 확인
			if (Child && Child->GetRootComponent() && Child->GetRootComponent()->GetAttachSocketName() == TEXT("Hand_R_Socket"))
			{
				// 1. 강제 분리
				Child->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

				// 2. 물리/충돌 복구
				if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(Child->GetRootComponent()))
				{
					RootComp->SetCollisionProfileName(TEXT("Pawn")); // 캐릭터는 Pawn, 사물은 PhysicsActor
					RootComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
					RootComp->SetSimulatePhysics(true);
				}

				// 3. [★중요] 캐릭터라면 메쉬 위치를 발바닥으로 내려줘야 보입니다!
				if (ACharacter* Victim = Cast<ACharacter>(Child))
				{
					// 상태 복구
					SetHeldState(Victim, false);

					// 메쉬 위치 원상복구 (이게 없으면 배꼽에 메쉬가 박혀서 안 보임)
					if (Victim->GetMesh())
					{
						Victim->GetMesh()->SetRelativeLocationAndRotation(
							FVector(0.f, 0.f, -90.f),  // 맨니/퀸 기본 발바닥 위치
							FRotator(0.f, -90.f, 0.f)  // 기본 회전
						);
					}

					// 무브먼트 복구
					if (Victim->GetCharacterMovement())
					{
						Victim->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
					}
				}
			}
		}
	}
}

void ATitanCharacter::PerformRadialImpact(FVector Origin, float Radius, float Damage, float RadialForce, AActor* IgnoreTarget)
{
	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(this); // 시전자(타이탄) 제외

	// [추가] 던져진 녀석(본인)도 폭발 대상에서 제외
	if (IgnoreTarget)
	{
		IgnoreActors.Add(IgnoreTarget);
	}

	TArray<AActor*> OverlappedActors;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));

	UKismetSystemLibrary::SphereOverlapActors(
		GetWorld(),
		Origin,
		Radius,
		ObjectTypes,
		AActor::StaticClass(),
		IgnoreActors,
		OverlappedActors
	);

	// 디버그용 원 그리기 (테스트 끝나면 주석 처리하세요)
	// DrawDebugSphere(GetWorld(), Origin, Radius, 12, FColor::Red, false, 2.0f);

	for (AActor* Victim : OverlappedActors)
	{
		if (!Victim || !Victim->IsValidLowLevel()) continue;

		

		// 1. 데미지 적용
		if (!Victim->ActorHasTag("Player"))
		{
			UGameplayStatics::ApplyDamage(Victim, Damage, GetController(), this, UDamageType::StaticClass());
		}
		// 2. 넉백 방향 계산 (폭발 중심 -> 피해자 방향)
		FVector LaunchDir = (Victim->GetActorLocation() - Origin).GetSafeNormal();
		LaunchDir.Z = 0.5f; // 위로 살짝 띄워줌 (0.5 정도가 적당)
		LaunchDir.Normalize();

		// 3. 물리적 넉백 적용
		if (ACharacter* VictimChar = Cast<ACharacter>(Victim))
		{
			// 캐릭터는 LaunchCharacter 사용
			VictimChar->LaunchCharacter(LaunchDir * RadialForce, true, true);
		}
		else if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(Victim->GetRootComponent()))
		{
			// 물체는 AddImpulse 사용
			if (RootComp->IsSimulatingPhysics())
			{
				RootComp->AddImpulse(LaunchDir * RadialForce * 100.0f);
			}
		}
	}
}

void ATitanCharacter::OnThrownActorHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit)
{
	if (OtherActor == this) return;

	if (SelfActor)
	{
		SelfActor->OnActorHit.RemoveDynamic(this, &ATitanCharacter::OnThrownActorHit);
	}

	// [힘 조절 설정]
	float ImpactRadius = 300.0f;  // 범위: 350 -> 300 (약간 줄임)
	float KnockbackForce = 800.0f; // 힘: 2000 -> 800 (대폭 줄임)
	float ImpactDamage = (CurrentSkillDamage > 0.0f) ? CurrentSkillDamage : 30.0f;

	// [중요] 마지막 인자에 SelfActor(던져진 애)를 넣어서 걔는 폭발 안 맞게 함
	PerformRadialImpact(Hit.ImpactPoint, ImpactRadius, ImpactDamage, KnockbackForce, SelfActor);

	// 던져진 녀석은 폭발에 안 휘말리고, 제자리에서 멈추며 일어남
	if (ACharacter* ThrownChar = Cast<ACharacter>(SelfActor))
	{
		// 멈추는 동작을 확실하게 하기 위해 속도 0으로 초기화
		if (ThrownChar->GetCharacterMovement())
		{
			ThrownChar->GetCharacterMovement()->StopMovementImmediately();
		}
		RecoverCharacter(ThrownChar);
	}
}

void ATitanCharacter::Multicast_ForceThrowCleanup_Implementation(AActor* TargetActor, FVector Velocity)
{
    if (!TargetActor || !TargetActor->IsValidLowLevel()) return;

    // =========================================================
    // [공통] 무조건 내 손에서 떼어내기 (가장 중요)
    // =========================================================
    TargetActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    // =========================================================
    // [CASE A] 캐릭터일 경우 (기존 로직 유지)
    // =========================================================
    if (ACharacter* Victim = Cast<ACharacter>(TargetActor))
    {
        // 1. 메쉬 위치 원상복구
        if (Victim->GetMesh())
        {
            Victim->GetMesh()->SetRelativeLocationAndRotation(
                FVector(0.f, 0.f, -90.f),
                FRotator(0.f, -90.f, 0.f)
            );
        }

        // 2. 물리 끄고 무브먼트 켜기
        if (Victim->GetCapsuleComponent())
        {
            Victim->GetCapsuleComponent()->SetSimulatePhysics(false);
            Victim->GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
            Victim->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        }

        if (Victim->GetCharacterMovement())
        {
            Victim->GetCharacterMovement()->StopMovementImmediately();
            Victim->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
            Victim->GetCharacterMovement()->GravityScale = 1.0f;
        }

        // 3. 잡기 상태 해제
        SetHeldState(Victim, false);

        // 4. 시각적 날리기 (내 캐릭터가 아닐 때만)
        if (!Victim->IsLocallyControlled())
        {
            Victim->LaunchCharacter(Velocity, true, true);
        }
    }
    // =========================================================
    // [CASE B] 버섯(물리 액터)일 경우 (🔥 여기가 핵심 수정!)
    // =========================================================
    else if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(TargetActor->GetRootComponent()))
    {
        // 1. 클라이언트에서도 물리 켜기 (이게 안 켜져서 둥둥 떴던 것임)
        RootComp->SetSimulatePhysics(true);
        RootComp->SetCollisionProfileName(TEXT("PhysicsActor")); // 혹은 BlockAllDynamic
        RootComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        
        // 2. 잠자고 있는 물리를 깨움 (필수)
        RootComp->WakeAllRigidBodies();

        // 3. 충돌 무시 설정 (던지는 순간 내 몸에 끼지 않게)
        if (GetCapsuleComponent())
        {
            GetCapsuleComponent()->IgnoreComponentWhenMoving(RootComp, true);
            RootComp->IgnoreComponentWhenMoving(GetCapsuleComponent(), true);
        }

        // 4. [시각 보정] 클라이언트에서도 힘을 가해서 서버 위치와 비슷하게 날아가도록 함
        RootComp->AddImpulse(Velocity, NAME_None, true);
    }
}

void ATitanCharacter::Multicast_PlayJobMontage_Implementation(FName SectionName) 
{ 
	ProcessSkill(TEXT("JobAbility"), SectionName); 
}



void ATitanCharacter::Server_ThrowTarget_Implementation(FVector ThrowStartLocation)
{
    if (!bIsGrabbing || !GrabbedActor) return;

    // 1. 타이머 및 몽타주 처리 (기존 유지)
    GetWorldTimerManager().ClearTimer(GrabTimerHandle);
    Multicast_PlayJobMontage(TEXT("Throw"));

    // =================================================================
    // [1] 던지는 힘과 방향 계산 (궤적 시스템과 싱크 맞추기)
    // =================================================================
    FRotator ThrowRotation = GetControlRotation();
    FVector ThrowDir = ThrowRotation.Vector();
    
    // 바닥 패대기 방지 (기존 유지)
    if (ThrowDir.Z < -0.1f) { ThrowDir.Z = -0.1f; ThrowDir.Normalize(); }
    
    // 🔥 [수정 1] 궤적(UpdateTrajectory)에서 썼던 'TrajectoryZBias'를 여기서도 더해야 함!
    // 그래야 빨간 선이랑 똑같은 각도로 날아갑니다.
    FVector FinalThrowDir = (ThrowDir + FVector(0.f, 0.f, TrajectoryZBias)).GetSafeNormal();
    
    // 기본 속도 계산
    FVector BaseVelocity = FinalThrowDir * ThrowForce;

    // =================================================================
    // [2] 시작 위치 보정 (기존 유지)
    // =================================================================
    FVector ExactStartLoc = GetActorLocation(); 
    FName SocketName = TEXT("Hand_R_Socket");
    if (GetMesh() && GetMesh()->DoesSocketExist(SocketName))
    {
        // 손 위치 + 앞쪽 30cm
        ExactStartLoc = GetMesh()->GetSocketLocation(SocketName) + (GetActorForwardVector() * 30.0f);
    }

    // =================================================================
    // [CASE A] 캐릭터 (몬스터 / 플레이어)
    // =================================================================
    if (ACharacter* Victim = Cast<ACharacter>(GrabbedActor))
    {
       // 1. 위치 강제 동기화
       Victim->SetActorLocation(ExactStartLoc, false, nullptr, ETeleportType::TeleportPhysics);
       
       // 2. 물리/충돌 상태 정리 (멀티캐스트)
       Multicast_ForceThrowCleanup(Victim, BaseVelocity);

       // 3. 무브먼트 세팅 (공기 저항 제거)
       if (Victim->GetCharacterMovement())
       {
           // 공기 저항(브레이크) 제거
           Victim->GetCharacterMovement()->BrakingDecelerationFalling = 0.0f; 
           // 중력 배율 1.0 고정
           Victim->GetCharacterMovement()->GravityScale = 1.0f;
           // 공중 제어 차단
           Victim->GetCharacterMovement()->AirControl = 0.0f; 
           
           // 🔥 [수정 2] 측면 마찰력 제거 (이게 있으면 공중에서 비비적대며 덜 날아감)
           Victim->GetCharacterMovement()->FallingLateralFriction = 0.0f;
       }

       // 🔥 [수정 3] 캐릭터용 힘 보정 (1.3배)
       // 캐릭터 무브먼트는 물리 엔진보다 저항이 심해서, 버섯과 똑같이 날리려면 힘을 더 줘야 합니다.
       float CharMultiplier = 1.3f; 

       // 4. 발사!
       Victim->LaunchCharacter(BaseVelocity * ThrowCharacterMultiplier, true, true);
       
       // 5. 후처리 델리게이트
       Victim->OnActorHit.RemoveDynamic(this, &ATitanCharacter::OnThrownActorHit);
       Victim->OnActorHit.AddDynamic(this, &ATitanCharacter::OnThrownActorHit);
       
       FTimerHandle RecoveryHandle;
       FTimerDelegate Delegate;
       Delegate.BindUFunction(this, TEXT("RecoverCharacter"), Victim);
       GetWorldTimerManager().SetTimer(RecoveryHandle, Delegate, 2.0f, false);
    }
    // =================================================================
    // [CASE B] 사물 (버섯 등)
    // =================================================================
    else if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(GrabbedActor->GetRootComponent()))
    {
       // 1. 위치 강제 동기화
       GrabbedActor->SetActorLocation(ExactStartLoc, false, nullptr, ETeleportType::TeleportPhysics);

       // 2. 물리 상태 정리
       Multicast_ForceThrowCleanup(GrabbedActor, BaseVelocity);
       GrabbedActor->SetReplicateMovement(true); 
       
       RootComp->SetCollisionProfileName(TEXT("PhysicsActor"));
       RootComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
       RootComp->SetSimulatePhysics(true);
       RootComp->WakeAllRigidBodies();
       
       // 3. 공기 저항 제거
       RootComp->SetLinearDamping(0.0f); 
       RootComp->SetAngularDamping(0.05f); 

       // 4. 충돌 무시 설정
       if (GetCapsuleComponent())
       {
          GetCapsuleComponent()->IgnoreComponentWhenMoving(RootComp, true);
          RootComp->IgnoreComponentWhenMoving(GetCapsuleComponent(), true);
       }
       if (GrabbedActor->ActorHasTag("Mushroom")) GrabbedActor->SetLifeSpan(5.0f);

       // 5. 발사! (버섯은 원래 힘 그대로)
       RootComp->SetPhysicsLinearVelocity(BaseVelocity); 

       // 6. 후처리
       GrabbedActor->OnActorHit.RemoveDynamic(this, &ATitanCharacter::OnThrownActorHit);
       GrabbedActor->OnActorHit.AddDynamic(this, &ATitanCharacter::OnThrownActorHit);

       if (ATitanThrowableActor* Throwable = Cast<ATitanThrowableActor>(GrabbedActor))
       {
          Throwable->OnThrown(FinalThrowDir);
       }
    }

    // 상태 초기화
    GrabbedActor = nullptr;
    bIsGrabbing = false;
    bIsCooldown = true;
    GetWorldTimerManager().SetTimer(CooldownTimerHandle, this, &ATitanCharacter::ResetCooldown, ThrowCooldownTime, false);
}

void ATitanCharacter::Server_Skill1_Implementation()
{
	if (!SkillDataTable) return;

	static const FString ContextString(TEXT("TitanSkill1"));
	FSkillData* Row = SkillDataTable->FindRow<FSkillData>(TEXT("Skill1"), ContextString);

	if (Row)
	{
		// 스킬 해금 여부 확인
		if (!IsSkillUnlocked(Row->RequiredStage)) return;

		// 데미지 정보 갱신 (나중에 노티파이에서 쓸 거임)
		CurrentSkillDamage = Row->Damage;

		// 쿨타임 설정
		if (Row->Cooldown > 0.0f) Skill1CooldownTime = Row->Cooldown;

		Multicast_Skill1();
	}

	// 쿨타임 타이머 (이건 유지)
	bIsSkill1Cooldown = true;
	GetWorldTimerManager().SetTimer(Skill1CooldownTimerHandle, this, &ATitanCharacter::ResetSkill1Cooldown, Skill1CooldownTime, false);
}

void ATitanCharacter::Multicast_Skill1_Implementation()
{
	// ProcessSkill을 호출하면 몽타주 재생 + PlayActionMontage(잠금)까지 한 번에 해결
	ProcessSkill(TEXT("Skill1"));
}

void ATitanCharacter::ThrowRock()
{
	if (!HeldRock) return;

	HeldRock->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(HeldRock->GetRootComponent()))
	{
		RootComp->SetSimulatePhysics(true);
		RootComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		FVector ThrowDir = GetControlRotation().Vector();
		ThrowDir = (ThrowDir + FVector(0, 0, 0.2f)).GetSafeNormal();

		RootComp->AddImpulse(ThrowDir * RockThrowForce * RootComp->GetMass());
	}
	HeldRock = nullptr;
}

// =================================================================
// [새로 추가] 애님 노티파이 연동 함수 구현
// =================================================================

// 1. 돌 생성 (노티파이가 호출 -> 서버에게 요청)
void ATitanCharacter::ExecuteSpawnRock()
{
	Server_SpawnRock();
}

// 2. 돌 생성 (서버 실제 구현)
void ATitanCharacter::Server_SpawnRock_Implementation()
{
	// 이미 들고 있으면 또 만들지 않음
	if (HeldRock) return;

	if (RockClass)
	{
		// 1. 소켓 결정 ("Rock_Socket"이 없으면 오른손 "Hand_R_Socket" 사용)
		FName SocketName = TEXT("Rock_Socket");
		if (!GetMesh()->DoesSocketExist(SocketName))
		{
			SocketName = TEXT("Hand_R_Socket");
		}

		FVector SocketLoc = GetMesh()->GetSocketLocation(SocketName);
		FRotator SocketRot = GetMesh()->GetSocketRotation(SocketName);

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;

		// 2. 돌 스폰
		HeldRock = GetWorld()->SpawnActor<ATitanRock>(RockClass, SocketLoc, SocketRot, SpawnParams);

		if (HeldRock)
		{
			// 3. 손에 붙이기 (물리 끄기)
			HeldRock->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);

			if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(HeldRock->GetRootComponent()))
			{
				RootComp->SetSimulatePhysics(false);
			}
			// 데미지 정보 전달
			HeldRock->InitializeRock(CurrentSkillDamage, this);
		}
	}
}

// 3. 돌 던지기 (노티파이가 호출 -> 서버에게 요청)
void ATitanCharacter::ExecuteThrowRock()
{
	Server_ThrowRock();
}

// 4. 돌 던지기 (서버 실제 구현)
void ATitanCharacter::Server_ThrowRock_Implementation()
{
	if (!HeldRock) return;

	// 1. 손에서 떼어내기
	HeldRock->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// 2. 물리 켜고 던지기
	if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(HeldRock->GetRootComponent()))
	{
		RootComp->SetSimulatePhysics(true);
		RootComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		// [핵심] 카메라가 보는 방향으로 던지기
		FRotator ThrowRot = GetControlRotation();
		FVector ThrowDir = ThrowRot.Vector();

		// 위쪽으로 살짝 보정 (포물선)
		ThrowDir = (ThrowDir + FVector(0, 0, 0.5f)).GetSafeNormal();

		// 힘 적용 (RockThrowForce 변수 사용)
		RootComp->AddImpulse(ThrowDir * RockThrowForce * RootComp->GetMass());
	}

	// 내 손을 떠났으니 변수 비우기
	HeldRock = nullptr;
}

void ATitanCharacter::Server_Skill2_Implementation()
{
	if (!SkillDataTable) return;

	static const FString ContextString(TEXT("Skill2 Context"));
	FSkillData* Row = SkillDataTable->FindRow<FSkillData>(TEXT("Skill2"), ContextString);

	if (Row)
	{
		if (!IsSkillUnlocked(Row->RequiredStage)) return;
		CurrentSkillDamage = Row->Damage;
		if (Row->Cooldown > 0.0f) Skill2CooldownTime = Row->Cooldown;
		Multicast_Skill2();
	}

	if (!bIsCharging)
	{
		bIsCharging = true;
		HitVictims.Empty();

		DefaultGroundFriction = GetCharacterMovement()->GroundFriction;
		DefaultGravityScale = GetCharacterMovement()->GravityScale;

		GetCharacterMovement()->GroundFriction = 0.0f;
		GetCharacterMovement()->GravityScale = 0.0f;
		GetCharacterMovement()->BrakingDecelerationFlying = 0.0f;

		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);

		FVector LaunchDir = GetActorForwardVector();
		LaunchCharacter(LaunchDir * 3000.f, true, true);

		bIsSkill2Cooldown = true;
		GetWorldTimerManager().SetTimer(Skill2CooldownTimerHandle, this, &ATitanCharacter::ResetSkill2Cooldown, Skill2CooldownTime, false);
		GetWorldTimerManager().SetTimer(ChargeTimerHandle, this, &ATitanCharacter::StopCharge, 0.3f, false);
	}
}

void ATitanCharacter::Multicast_Skill2_Implementation()
{
	ProcessSkill(TEXT("Skill2"));
}

void ATitanCharacter::OnChargeOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || !bIsCharging || OtherActor == this || HitVictims.Contains(OtherActor)) return;

	if (ACharacter* VictimChar = Cast<ACharacter>(OtherActor))
	{
		

		GetCapsuleComponent()->IgnoreActorWhenMoving(VictimChar, true);
		VictimChar->GetCapsuleComponent()->IgnoreActorWhenMoving(this, true);
		HitVictims.Add(VictimChar);

		if (!VictimChar->ActorHasTag("Player"))
		{
			UGameplayStatics::ApplyDamage(OtherActor, CurrentSkillDamage, GetController(), this, UDamageType::StaticClass());
		}
		FVector KnockbackDir = GetActorForwardVector();
		FVector LaunchForce = (KnockbackDir * 500.f) + FVector(0, 0, 1000.f);
		VictimChar->LaunchCharacter(LaunchForce, true, true);
	}
}

void ATitanCharacter::OnChargeHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority() || !bIsCharging) return;
	if (OtherActor->IsRootComponentStatic())
	{
		StopCharge();
	}
}

void ATitanCharacter::ProcessSkill(FName SkillRowName, FName StartSectionName)
{
	if (!SkillDataTable) return;
	static const FString ContextString(TEXT("TitanSkillContext"));
	FSkillData* Data = SkillDataTable->FindRow<FSkillData>(SkillRowName, ContextString);

	if (Data && Data->SkillMontage)
	{
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		if (AnimInstance)
		{
			if (AnimInstance->Montage_IsPlaying(Data->SkillMontage) && StartSectionName != NAME_None)
			{
				AnimInstance->Montage_JumpToSection(StartSectionName, Data->SkillMontage);
			}
			else
			{
				PlayActionMontage(Data->SkillMontage);
				// 만약 PlayActionMontage가 섹션 인자를 안 받는다면, 재생 직후 점프
				if (StartSectionName != NAME_None)
				{
					AnimInstance->Montage_JumpToSection(StartSectionName, Data->SkillMontage);
				}
			}
		}
	}
}

void ATitanCharacter::StopCharge()
{
	bIsCharging = false;
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->GroundFriction = DefaultGroundFriction;
	GetCharacterMovement()->GravityScale = DefaultGravityScale;

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);

	for (AActor* IgnoredActor : HitVictims)
	{
		if (IgnoredActor)
		{
			GetCapsuleComponent()->IgnoreActorWhenMoving(IgnoredActor, false);
			if (ACharacter* IgnoredChar = Cast<ACharacter>(IgnoredActor))
			{
				IgnoredChar->GetCapsuleComponent()->IgnoreActorWhenMoving(this, false);
			}
		}
	}
	HitVictims.Empty();
}

void ATitanCharacter::UpdateHoverHighlight()
{
	if (!Camera) return;

	FVector Start = Camera->GetComponentLocation();
	FVector End = Start + (Camera->GetForwardVector() * 600.0f);
	FCollisionShape Shape = FCollisionShape::MakeSphere(70.0f);
	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	
	// 윈드존이나 트리거 박스가 보통 WorldDynamic이라서 시야를 가립니다.
	// 이걸 빼면 투명 박스를 뚫고 뒤에 있는 몬스터/버섯을 잡습니다.
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);          // 몬스터, 플레이어
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);   // 버섯 (PhysicsActor)

	bool bHit = GetWorld()->SweepSingleByObjectType(
	   HitResult, 
	   Start, 
	   End, 
	   FQuat::Identity, 
	   ObjectParams, 
	   Shape, 
	   Params
	);

	
	AActor* NewTarget = nullptr;

	if (bHit && HitResult.GetActor())
	{
		AActor* HitActor = HitResult.GetActor();

		// 1. 기본 체크 (캐릭터여부, 던질수있는 액터여부, 버섯 태그)
		bool bIsTargetChar = (Cast<ACharacter>(HitActor) != nullptr);
		bool bIsThrowable = (Cast<ATitanThrowableActor>(HitActor) != nullptr);
		bool bIsMushroom = HitActor->ActorHasTag("Mushroom");

		// 2. [추가] 보스 클래스 체크 (헤더에 해당 보스 클래스들이 include 되어 있어야 합니다)
		bool bIsMidBoss = HitActor->IsA(AEnemyMidBoss::StaticClass());
		bool bIsStageBoss = HitActor->IsA(AStageBossBase::StaticClass());

		// 3. 태그 체크 (기존 유지)
		bool bHasBossTag = HitActor->ActorHasTag("Boss") || HitActor->ActorHasTag("MidBoss");

		// 보스나 중간보스가 아닐 때만 타겟으로 지정
		if ((bIsTargetChar || bIsThrowable || bIsMushroom) && !bIsMidBoss && !bIsStageBoss && !bHasBossTag)
		{
			NewTarget = HitActor;
		}
	}

	if (HoveredActor != NewTarget)
	{
		if (HoveredActor) SetHighlight(HoveredActor, false);
		if (NewTarget) SetHighlight(NewTarget, true);
		HoveredActor = NewTarget;
	}
}



void ATitanCharacter::SetHighlight(AActor* Target, bool bEnable)
{
	if (!Target) return;
	TArray<UPrimitiveComponent*> Components;
	Target->GetComponents<UPrimitiveComponent>(Components);

	for (auto Comp : Components)
	{
		Comp->SetRenderCustomDepth(bEnable);
		if (bEnable) Comp->SetCustomDepthStencilValue(250);
	}
}

void ATitanCharacter::RecoverCharacter(ACharacter* Victim)
{
	if (!Victim || !Victim->IsValidLowLevel()) return;

	// ... (기존 메쉬/콜리전 복구 코드 유지) ...
	Multicast_FixMesh(Victim);
	if (Victim->GetCapsuleComponent()) 
		Victim->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	if (Victim->GetCharacterMovement())
	{
		Victim->GetCharacterMovement()->StopMovementImmediately();
		Victim->GetCharacterMovement()->Velocity = FVector::ZeroVector;

		// 🔥 [복구] 던져질 때 껐던 기능들을 다시 켭니다.
		// (원래 값이 있다면 그 값을 저장해뒀다 쓰는 게 좋지만, 하드코딩으로도 충분합니다)
		Victim->GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f; // 기본 제동력
		Victim->GetCharacterMovement()->GravityScale = 1.0f; // 기본 중력
		Victim->GetCharacterMovement()->AirControl = 0.05f;  // 기본 공중 제어

		// 땅 파고듦 방지 후 걷기 모드 전환
		FVector FixLoc = Victim->GetActorLocation() + FVector(0.f, 0.f, 5.0f);
		Victim->SetActorLocation(FixLoc, false, nullptr, ETeleportType::TeleportPhysics);
		Victim->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}

	Victim->ForceNetUpdate();
	// AI 재시작
	AAIController* AIC = Cast<AAIController>(Victim->GetController());
	if (AIC && AIC->GetBrainComponent()) AIC->GetBrainComponent()->RestartLogic();
}

void ATitanCharacter::SetHeldState(ACharacter* Target, bool bIsHeld)
{
	if (!Target) return;

	AController* TargetCon = Target->GetController();
	AAIController* AIC = Cast<AAIController>(TargetCon);
	UCharacterMovementComponent* CMC = Target->GetCharacterMovement();
	UCapsuleComponent* Capsule = Target->GetCapsuleComponent();

	if (bIsHeld)
	{
		Target->StopAnimMontage();
		if (CMC)
		{
			CMC->StopMovementImmediately();
			CMC->DisableMovement();
		}
		if (Capsule) Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (AIC && AIC->GetBrainComponent())
		{
			AIC->GetBrainComponent()->StopLogic(TEXT("Grabbed"));
			AIC->StopMovement();
		}
	}
	else
	{
		if (Capsule) Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		if (CMC) CMC->SetMovementMode(MOVE_Falling);
	}
}

void ATitanCharacter::ResetCooldown()
{
	bIsCooldown = false;
}

void ATitanCharacter::ResetSkill1Cooldown()
{
	bIsSkill1Cooldown = false;
}

void ATitanCharacter::ResetSkill2Cooldown()
{
	bIsSkill2Cooldown = false;
}

void ATitanCharacter::ShowTrajectory(bool bShow)
{
	// 스플라인 자체 끄기
	if (TrajectorySpline) TrajectorySpline->SetVisibility(bShow);

	// [수정] 변수명 Mesh -> SplineMesh로 변경 (ACharacter의 Mesh 변수와 이름 충돌 방지)
	for (USplineMeshComponent* SplineMesh : SplineMeshes)
	{
		if (SplineMesh) SplineMesh->SetVisibility(bShow);
	}
}

void ATitanCharacter::UpdateTrajectory()
{
    if (!TrajectorySpline || !GrabbedActor) return;

    // 1. [물리 예측 계산]
    FPredictProjectilePathParams PredictParams;

    FName SocketName = TEXT("Hand_R_Socket");

    if (GetMesh() && GetMesh()->DoesSocketExist(SocketName))
    {
        // 🔥 [보정 1] 시작 위치를 손보다 살짝(30cm) 앞으로 당김
        // (실제 물체는 던져질 때 충돌 방지를 위해 몸 밖으로 밀려나는데, 궤적도 이걸 맞춰줘야 함)
        FVector HandLoc = GetMesh()->GetSocketLocation(SocketName);
        FVector ForwardOffset = GetActorForwardVector() * 30.0f; 
        PredictParams.StartLocation = HandLoc + ForwardOffset; 
    }
    else
    {
        PredictParams.StartLocation = ThrowSpawnPoint->GetComponentLocation();
    }
    // 나 자신 무시 (필수)
    PredictParams.ActorsToIgnore.Add(this);
	PredictParams.ActorsToIgnore.Add(GrabbedActor); // 잡은 몬스터 버섯 무시
	
    // 던지는 힘과 방향 계산 (기존과 동일)
    FRotator ThrowRotation = GetControlRotation();
    FVector ThrowDir = ThrowRotation.Vector();
    // 바닥 패대기 방지
    if (ThrowDir.Z < -0.1f) { ThrowDir.Z = -0.1f; ThrowDir.Normalize(); }
    
	FVector FinalThrowDir = (ThrowDir + FVector(0.f, 0.f, TrajectoryZBias)).GetSafeNormal();
    
	PredictParams.LaunchVelocity = FinalThrowDir * ThrowForce;
    
    //  시간 늘리기 (2초 -> 5초)
    // 높게 던지면 2초 넘게 날아가는데, 궤적이 중간에 끊기는 걸 방지합니다.
    PredictParams.MaxSimTime = 5.0f;       
    
    //  판정 크기 축소 (10.0 -> 2.0)
    // 이게 너무 크면 땅에 닿지도 않았는데 "어? 바닥이네" 하고 궤적을 끊어버립니다.
    // 아주 얇게 해서 땅속 깊이 꽂히게 해야 실제 낙하 지점과 맞습니다.
    PredictParams.ProjectileRadius = 30.0f;      

    //  [보정 4] 계산 빈도 증가 
    PredictParams.SimFrequency = 60.0f;    

    // =================================================================

	PredictParams.OverrideGravityZ = GetWorld()->GetGravityZ() * TrajectoryGravityScale;
	
    PredictParams.bTraceWithCollision = true; 
    PredictParams.bTraceWithChannel = true;
    PredictParams.TraceChannel = ECC_WorldStatic; 
    PredictParams.DrawDebugType = EDrawDebugTrace::None; 

    FPredictProjectilePathResult PredictResult;
    UGameplayStatics::PredictProjectilePath(this, PredictParams, PredictResult);


    // 2. [스플라인 업데이트]
    TrajectorySpline->ClearSplinePoints(false);
    
    for (const FPredictProjectilePathPointData& PointData : PredictResult.PathData)
    {
        TrajectorySpline->AddSplinePoint(PointData.Location, ESplineCoordinateSpace::World, false);
    }
    TrajectorySpline->UpdateSpline();


    // 3. [시각화: 스플라인 메쉬 배치]
    const int32 PointCount = TrajectorySpline->GetNumberOfSplinePoints();
    
    for (int32 i = 0; i < PointCount - 1; i++)
    {
        // 풀링(Pooling) 로직
        if (SplineMeshes.Num() <= i)
        {
            USplineMeshComponent* NewMesh = NewObject<USplineMeshComponent>(this);
            NewMesh->SetMobility(EComponentMobility::Movable);
            NewMesh->SetupAttachment(TrajectorySpline);
            
            if (TrajectoryMesh) NewMesh->SetStaticMesh(TrajectoryMesh);
            if (TrajectoryMaterial) NewMesh->SetMaterial(0, TrajectoryMaterial);
            
            NewMesh->RegisterComponent();
            SplineMeshes.Add(NewMesh);
        }

        USplineMeshComponent* CurrentMesh = SplineMeshes[i];
        CurrentMesh->SetVisibility(true);

        FVector StartLoc, StartTan, EndLoc, EndTan;
        TrajectorySpline->GetLocationAndTangentAtSplinePoint(i, StartLoc, StartTan, ESplineCoordinateSpace::Local);
        TrajectorySpline->GetLocationAndTangentAtSplinePoint(i + 1, EndLoc, EndTan, ESplineCoordinateSpace::Local);

        // 선 두께 얇게 (0.04f)
        FVector2D ScaleValue = FVector2D(0.04f, 0.04f); 
        CurrentMesh->SetStartScale(ScaleValue);
        CurrentMesh->SetEndScale(ScaleValue);

        CurrentMesh->SetStartAndEnd(StartLoc, StartTan, EndLoc, EndTan, true);
    }
    
    // 남은 메쉬 숨기기
    for (int32 i = PointCount - 1; i < SplineMeshes.Num(); i++)
    {
        SplineMeshes[i]->SetVisibility(false);
    }
}