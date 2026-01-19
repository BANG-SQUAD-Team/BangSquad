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
#include "Project_Bang_Squad/Character/Enemy/EnemyNormal.h"
#include "Project_Bang_Squad/Character/Player/Titan/TitanRock.h"

// [주의] BoxComponent 관련 헤더 및 코드는 삭제됨 (팔라딘 방식 Trace 사용)

ATitanCharacter::ATitanCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

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
}

void ATitanCharacter::BeginPlay()
{
	Super::BeginPlay();

	GetCapsuleComponent()->OnComponentBeginOverlap.AddDynamic(this, &ATitanCharacter::OnChargeOverlap);
	GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &ATitanCharacter::OnChargeHit);
}

void ATitanCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IsLocallyControlled() && !bIsGrabbing)
	{
		UpdateHoverHighlight();
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

	// [추가] 공격 판정 타이머 정리
	GetWorldTimerManager().ClearTimer(AttackHitTimerHandle);
	GetWorldTimerManager().ClearTimer(HitLoopTimerHandle);

	StopAnimMontage();
	Super::OnDeath();
}

// =================================================================
// [입력 핸들러]
// =================================================================

void ATitanCharacter::Attack()
{
	if (!CanAttack()) return;

	FName SkillRowName = bIsNextAttackA ? TEXT("Attack_A") : TEXT("Attack_B");

	// [수정] 클라이언트 선제적 쿨타임 적용 (데이터 테이블 조회)
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

// ... (이하 기존 스킬 구현 유지) ...

void ATitanCharacter::JobAbility()
{
	// 로컬 체크
	if (bIsDead || bIsCooldown) return;

	if (bIsGrabbing)
	{
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
		if (HoveredActor) Server_TryGrab(HoveredActor);
	}
}

void ATitanCharacter::Skill1()
{
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
	float DistSq = FVector::DistSquared(GetActorLocation(), TargetToGrab->GetActorLocation());
	if (DistSq > 1000.f * 1000.f) return;
	GrabbedActor = TargetToGrab;
	bIsGrabbing = true;
	GrabbedActor->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Hand_R_Socket"));
	if (ACharacter* Victim = Cast<ACharacter>(GrabbedActor)) { SetHeldState(Victim, true); }
	else if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(GrabbedActor->GetRootComponent())) { RootComp->SetSimulatePhysics(false); }
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
		bIsGrabbing = true; 
		if (ACharacter* Victim = Cast<ACharacter>(GrabbedActor)) 
		{ 
			SetHeldState(Victim, true); 
		} 
		else if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(GrabbedActor->GetRootComponent())) 
		{ 
			RootComp->SetSimulatePhysics(false); 
		} 
		GrabbedActor->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Hand_R_Socket")); 
	} 
	else 
	{ 
		bIsGrabbing = false; 
	} 
}

void ATitanCharacter::Multicast_PlayJobMontage_Implementation(FName SectionName) 
{ 
	ProcessSkill(TEXT("JobAbility"), SectionName); 
}

void ATitanCharacter::Server_ThrowTarget_Implementation(FVector ThrowStartLocation) 
{ 
	if (!bIsGrabbing || !GrabbedActor) return;
	GetWorldTimerManager().ClearTimer(GrabTimerHandle);
	if (SkillDataTable) 
	{
		static const FString ContextString(TEXT("TitanThrowContext")); 
		FSkillData* Row = SkillDataTable->FindRow<FSkillData>(TEXT("JobAbility"), ContextString); 
		if (Row && Row->Cooldown > 0.0f) ThrowCooldownTime = Row->Cooldown; 
	}

	Multicast_PlayJobMontage(TEXT("Throw")); 
	GrabbedActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform); 
	GrabbedActor->SetActorLocation(ThrowStartLocation, false, nullptr, ETeleportType::TeleportPhysics); 
	FRotator ActorRot = GrabbedActor->GetActorRotation(); 
	GrabbedActor->SetActorRotation(FRotator(0.f, ActorRot.Yaw, 0.f)); 
	FRotator ThrowRotation = GetControlRotation(); FVector ThrowDir = ThrowRotation.Vector(); 
	if (ACharacter* Victim = Cast<ACharacter>(GrabbedActor)) 
	{ 
		SetHeldState(Victim, false); FVector FinalThrowDir = (ThrowDir + FVector(0.f, 0.f, 0.25f)).GetSafeNormal(); 
		Victim->LaunchCharacter(FinalThrowDir * ThrowForce, true, true); 
		FTimerHandle RecoveryHandle; FTimerDelegate Delegate; 
		Delegate.BindUFunction(this, TEXT("RecoverCharacter"), Victim); 
		GetWorldTimerManager().SetTimer(RecoveryHandle, Delegate, 1.5f, false); 
	} 
	else if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(GrabbedActor->GetRootComponent())) 
	{ 
		RootComp->SetSimulatePhysics(true); RootComp->AddImpulse(ThrowDir * ThrowForce * 50.f); 
	} 
	GrabbedActor = nullptr; 
	bIsGrabbing = false; 
	bIsCooldown = true; GetWorldTimerManager().SetTimer(CooldownTimerHandle, this, &ATitanCharacter::ResetCooldown, ThrowCooldownTime, false); 
}

void ATitanCharacter::Server_Skill1_Implementation()
{
	if (!SkillDataTable) return;

	static const FString ContextString(TEXT("TitanSkill1"));
	FSkillData* Row = SkillDataTable->FindRow<FSkillData>(TEXT("Skill1"), ContextString);

	if (Row)
	{
		if (!IsSkillUnlocked(Row->RequiredStage)) return;

		CurrentSkillDamage = Row->Damage;
		if (Row->Cooldown > 0.0f) Skill1CooldownTime = Row->Cooldown;
		if (Row->SkillMontage) PlayAnimMontage(Row->SkillMontage);
	}

	if (RockClass)
	{
		FVector SocketLoc = GetMesh()->GetSocketLocation(TEXT("Hand_R_Socket"));
		FRotator SocketRot = GetMesh()->GetSocketRotation(TEXT("Hand_R_Socket"));

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;

		HeldRock = GetWorld()->SpawnActor<ATitanRock>(RockClass, SocketLoc, SocketRot, SpawnParams);

		if (HeldRock)
		{
			HeldRock->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Hand_R_Socket"));
			if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(HeldRock->GetRootComponent()))
			{
				RootComp->SetSimulatePhysics(false);
			}
			HeldRock->InitializeRock(CurrentSkillDamage, this);
		}
	}

	GetWorldTimerManager().SetTimer(RockThrowTimerHandle, this, &ATitanCharacter::ThrowRock, 0.8f, false);
	bIsSkill1Cooldown = true;
	GetWorldTimerManager().SetTimer(Skill1CooldownTimerHandle, this, &ATitanCharacter::ResetSkill1Cooldown, Skill1CooldownTime, false);
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
		if (Row->SkillMontage) PlayAnimMontage(Row->SkillMontage);
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

void ATitanCharacter::OnChargeOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || !bIsCharging || OtherActor == this || HitVictims.Contains(OtherActor)) return;

	if (ACharacter* VictimChar = Cast<ACharacter>(OtherActor))
	{
		if (VictimChar->ActorHasTag("Player")) return;

		GetCapsuleComponent()->IgnoreActorWhenMoving(VictimChar, true);
		VictimChar->GetCapsuleComponent()->IgnoreActorWhenMoving(this, true);
		HitVictims.Add(VictimChar);

		UGameplayStatics::ApplyDamage(OtherActor, CurrentSkillDamage, GetController(), this, UDamageType::StaticClass());

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
				PlayAnimMontage(Data->SkillMontage, 1.0f, StartSectionName);
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

	bool bHit = GetWorld()->SweepSingleByChannel(HitResult, Start, End, FQuat::Identity, ECC_Pawn, Shape, Params);
	AActor* NewTarget = nullptr;

	if (bHit && HitResult.GetActor())
	{
		AActor* HitActor = HitResult.GetActor();
		if (Cast<ACharacter>(HitActor) && !HitActor->ActorHasTag("Boss") && !HitActor->ActorHasTag("MidBoss"))
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

	FRotator CurrentRot = Victim->GetActorRotation();
	Victim->SetActorRotation(FRotator(0.f, CurrentRot.Yaw, 0.f));

	if (Victim->GetCapsuleComponent())
	{
		Victim->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	if (Victim->GetCharacterMovement())
	{
		if (Victim->GetCharacterMovement()->MovementMode == MOVE_None || Victim->GetCharacterMovement()->MovementMode == MOVE_Falling)
		{
			Victim->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
		Victim->GetCharacterMovement()->StopMovementImmediately();
	}

	AAIController* AIC = Cast<AAIController>(Victim->GetController());
	if (AIC && AIC->GetBrainComponent())
	{
		AIC->GetBrainComponent()->RestartLogic();
	}
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