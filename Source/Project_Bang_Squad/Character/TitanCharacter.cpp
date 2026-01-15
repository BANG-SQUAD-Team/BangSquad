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


ATitanCharacter::ATitanCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
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
	if (!bIsGrabbing) { UpdateHoverHighlight(); }
}

void ATitanCharacter::OnDeath()
{
	if (bIsDead) return;

	if (bIsGrabbing && GrabbedActor)
	{
		GrabbedActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

		if (ACharacter* Victim = Cast<ACharacter>(GrabbedActor))
		{
			SetHeldState(Victim, false);
			if (Victim->GetCharacterMovement())
			{
				Victim->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			}
		}
		else if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(GrabbedActor))
		{
			RootComp->SetSimulatePhysics(true);
		}

		GrabbedActor = nullptr;
		bIsGrabbing = false;
		GetWorldTimerManager().ClearTimer(GrabTimerHandle);
	}

	if (bIsCharging)
	{
		StopCharge();
		GetWorldTimerManager().ClearTimer(ChargeTimerHandle);
	}

	if (HoveredActor)
	{
		SetHighlight(HoveredActor, false);
		HoveredActor = nullptr;
	}

	GetWorldTimerManager().ClearTimer(CooldownTimerHandle);
	GetWorldTimerManager().ClearTimer(Skill2CooldownTimerHandle);
	GetWorldTimerManager().ClearTimer(Skill1CooldownTimerHandle);

	StopAnimMontage();
	Super::OnDeath();
}

void ATitanCharacter::ProcessSkill(FName SkillRowName, FName StartSectionName)
{
	if (!SkillDataTable) { return; }

	static const FString ContextString(TEXT("TitanSkillContext"));
	FSkillData* Data = SkillDataTable->FindRow<FSkillData>(SkillRowName, ContextString);

	if (Data)
	{
		if (!IsSkillUnlocked(Data->RequiredStage))
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red,
				FString::Printf(TEXT("Skill Locked! (Required Stage: %d)"), Data->RequiredStage));
			return;
		}

		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan,
			FString::Printf(TEXT("Titan %s Activated!"), *Data->SkillName.ToString()));

		if (Data->SkillMontage)
		{
			UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
			if (AnimInstance)
			{
				if (AnimInstance->Montage_IsPlaying(Data->SkillMontage))
				{
					if (StartSectionName != NAME_None)
					{
						AnimInstance->Montage_JumpToSection(StartSectionName, Data->SkillMontage);
					}
				}
				else
				{
					PlayAnimMontage(Data->SkillMontage, 1.0f, StartSectionName);
				}
			}
		}
	}
}

void ATitanCharacter::TryGrab()
{
	if (!HoveredActor) return;

	GrabbedActor = HoveredActor;
	HoveredActor = nullptr;
	bIsGrabbing = true;

	SetHighlight(GrabbedActor, true);
	ProcessSkill(TEXT("JobAbility"), TEXT("Grab"));
}

void ATitanCharacter::ExecuteGrab()
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Magenta, TEXT("ExecuteGrab Called!"));

	if (!GrabbedActor || !bIsGrabbing) return;

	if (ACharacter* Victim = Cast<ACharacter>(GrabbedActor))
	{
		SetHeldState(Victim, true);
	}
	else
	{
		if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(GrabbedActor->GetRootComponent()))
		{
			RootComp->SetSimulatePhysics(false);
		}
	}

	GrabbedActor->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Hand_R_Socket"));

	GetWorldTimerManager().SetTimer(GrabTimerHandle, this, &ATitanCharacter::ThrowTarget, GrabMaxDuration, false);
}

void ATitanCharacter::ThrowTarget()
{
	if (!bIsGrabbing || !GrabbedActor) return;

	GetWorldTimerManager().ClearTimer(GrabTimerHandle);

	// [데이터 테이블 쿨타임 적용]
	if (SkillDataTable)
	{
		static const FString ContextString(TEXT("TitanThrowContext"));
		FSkillData* Row = SkillDataTable->FindRow<FSkillData>(TEXT("JobAbility"), ContextString);
		if (Row && Row->Cooldown > 0.0f)
		{
			ThrowCooldownTime = Row->Cooldown;
		}
	}

	ProcessSkill(TEXT("JobAbility"), TEXT("Throw"));
	SetHighlight(GrabbedActor, false);

	GrabbedActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	FRotator ActorRot = GrabbedActor->GetActorRotation();
	GrabbedActor->SetActorRotation(FRotator(0.f, ActorRot.Yaw, 0.f));
	GrabbedActor->AddActorWorldOffset(FVector(0.f, 0.f, 50.f), false, nullptr, ETeleportType::TeleportPhysics);

	if (ACharacter* Victim = Cast<ACharacter>(GrabbedActor))
	{
		SetHeldState(Victim, false);

		FVector ThrowDir = GetControlRotation().Vector();
		ThrowDir = (ThrowDir + FVector(0.f, 0.f, 0.25f)).GetSafeNormal();
		FVector LaunchVelocity = ThrowDir * ThrowForce;

		Victim->LaunchCharacter(LaunchVelocity, true, true);
	}
	else
	{
		if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(GrabbedActor->GetRootComponent()))
		{
			RootComp->SetSimulatePhysics(true);
			RootComp->AddImpulse(GetControlRotation().Vector() * ThrowForce * 50.f);
		}
	}

	if (ACharacter* Victim = Cast<ACharacter>(GrabbedActor))
	{
		FTimerHandle RecoveryHandle;
		FTimerDelegate Delegate;
		Delegate.BindUFunction(this, TEXT("RecoverCharacter"), Victim);
		GetWorldTimerManager().SetTimer(RecoveryHandle, Delegate, 1.5f, false);
	}

	bIsGrabbing = false;
	GrabbedActor = nullptr;
	bIsCooldown = true;
	GetWorldTimerManager().SetTimer(CooldownTimerHandle, this, &ATitanCharacter::ResetCooldown, ThrowCooldownTime, false);
}

// [수정] 여기가 중요합니다. Striker 코드를 지우고 Titan 코드로 복구했습니다.
void ATitanCharacter::JobAbility()
{
	if (bIsDead) return;
	if (bIsCooldown)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, TEXT("Ability on Cooldown..."));
		return;
	}

	if (!bIsGrabbing) { TryGrab(); }
	else { ThrowTarget(); }
}

void ATitanCharacter::Attack()
{
	if (!CanAttack()) return;

	// [수정] A/B 공격 분기 처리
	FName SkillRowName = bIsNextAttackA ? TEXT("Attack_A") : TEXT("Attack_B");

	if (SkillDataTable)
	{
		static const FString ContextString(TEXT("TitanAttack"));
		// 동적으로 결정된 SkillRowName으로 데이터 조회
		FSkillData* Row = SkillDataTable->FindRow<FSkillData>(SkillRowName, ContextString);
		if (Row)
		{
			if (Row->Cooldown > 0.0f)
			{
				AttackCooldownTime = Row->Cooldown;
			}
		}
	}

	StartAttackCooldown();

	// [수정] 결정된 스킬 이름으로 실행
	ProcessSkill(SkillRowName);

	bIsNextAttackA = !bIsNextAttackA;
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
		ACharacter* TargetChar = Cast<ACharacter>(HitActor);

		if (TargetChar && !HitActor->ActorHasTag("Boss") && !HitActor->ActorHasTag("MidBoss"))
		{
			NewTarget = HitActor;
			if (GEngine) GEngine->AddOnScreenDebugMessage(1, 0.1f, FColor::Blue,
				FString::Printf(TEXT("Target Locked: %s"), *HitActor->GetName()));
		}
	}

	if (HoveredActor != NewTarget)
	{
		if (HoveredActor) SetHighlight(HoveredActor, false);
		if (NewTarget) SetHighlight(NewTarget, true);
		HoveredActor = NewTarget;
	}
}

void ATitanCharacter::RecoverCharacter(ACharacter* Victim)
{
	if (!Victim || !Victim->IsValidLowLevel()) return;

	FRotator CurrentRot = Victim->GetActorRotation();
	if (!FMath::IsNearlyZero(CurrentRot.Pitch) || !FMath::IsNearlyZero(CurrentRot.Roll))
	{
		Victim->SetActorRotation(FRotator(0.f, CurrentRot.Yaw, 0.f));
	}

	if (Victim->GetCapsuleComponent()->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics)
	{
		Victim->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	if (Victim->GetCharacterMovement())
	{
		if (Victim->GetCharacterMovement()->MovementMode == MOVE_None ||
			Victim->GetCharacterMovement()->MovementMode == MOVE_Falling)
		{
			Victim->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
		Victim->GetCharacterMovement()->StopMovementImmediately();
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
		if (AIC && AIC->GetBrainComponent())
		{
			AIC->GetBrainComponent()->RestartLogic();
		}
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

void ATitanCharacter::ResetCooldown()
{
	bIsCooldown = false;
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Green, TEXT("Job Ability Ready!"));
}

void ATitanCharacter::Skill1()
{
	if (bIsDead) return;

	// 쿨타임 체크
	if (bIsSkill1Cooldown)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, TEXT("Skill1 (Rock Throw) Cooldown!"));
		return;
	}

	// 서버로 요청
	Server_Skill1();
}

// [Skill 1: 서버 구현] 바위 생성 및 들어올리기
void ATitanCharacter::Server_Skill1_Implementation()
{
	if (!SkillDataTable) return;

	// 1. 데이터 테이블 조회
	static const FString ContextString(TEXT("TitanSkill1"));
	FSkillData* Row = SkillDataTable->FindRow<FSkillData>(TEXT("Skill1"), ContextString);

	if (Row)
	{
		if (!IsSkillUnlocked(Row->RequiredStage)) return;

		CurrentSkillDamage = Row->Damage;
		if (Row->Cooldown > 0.0f) Skill1CooldownTime = Row->Cooldown;

		// 2. 몽타주 재생 (바닥 집는 모션)
		if (Row->SkillMontage)
		{
			PlayAnimMontage(Row->SkillMontage);
		}
	}

	// 3. 바위 소환 (손 소켓에 붙여서 생성)
	if (RockClass)
	{
		FVector SocketLoc = GetMesh()->GetSocketLocation(TEXT("Hand_R_Socket"));
		FRotator SocketRot = GetMesh()->GetSocketRotation(TEXT("Hand_R_Socket"));

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;

		// 바위 생성
		HeldRock = GetWorld()->SpawnActor<ATitanRock>(RockClass, SocketLoc, SocketRot, SpawnParams);

		if (HeldRock)
		{
			// 손에 부착
			HeldRock->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Hand_R_Socket"));

			// 아직 던지기 전이므로 물리 끄기
			if (HeldRock->GetRootComponent())
			{
				UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(HeldRock->GetRootComponent());
				if (RootComp) RootComp->SetSimulatePhysics(false);
			}

			// 데미지 정보 전달
			HeldRock->InitializeRock(CurrentSkillDamage, this);
		}
	}

	// 4. 던지기 타이머 설정 
	// (애니메이션이 바위를 들어올려서 던지는 시점까지 대기. 예: 0.8초 뒤)
	// *더 정확하게 하려면 몽타주에 AnimNotify를 박아서 호출해야 합니다. 일단은 타이머로 처리.*
	GetWorldTimerManager().SetTimer(RockThrowTimerHandle, this, &ATitanCharacter::ThrowRock, 0.8f, false);

	// 5. 쿨타임 시작
	bIsSkill1Cooldown = true;
	GetWorldTimerManager().SetTimer(Skill1CooldownTimerHandle, this, &ATitanCharacter::ResetSkill1Cooldown, Skill1CooldownTime, false);
}

// [Skill 1: 투척] 바위를 손에서 떼고 날리기
void ATitanCharacter::ThrowRock()
{
	if (!HeldRock) return;

	// 1. 손에서 분리
	HeldRock->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// 2. 물리 활성화
	UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(HeldRock->GetRootComponent());
	if (RootComp)
	{
		RootComp->SetSimulatePhysics(true);
		RootComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		// 3. 던질 방향 계산 (카메라가 보는 방향)
		FVector ThrowDir = GetControlRotation().Vector();
		ThrowDir = (ThrowDir + FVector(0, 0, 0.2f)).GetSafeNormal(); // 약간 위로 던지게 보정

		// 4. 힘 가하기 (Impulse)
		RootComp->AddImpulse(ThrowDir * RockThrowForce * RootComp->GetMass());
	}

	// 참조 해제
	HeldRock = nullptr;

	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, TEXT("Rock Thrown!"));
}

void ATitanCharacter::ResetSkill1Cooldown()
{
	bIsSkill1Cooldown = false;
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Green, TEXT("Skill1 Ready!"));
}

// [수정] Skill2 진입점 함수가 빠져있어서 추가했습니다.
void ATitanCharacter::Skill2()
{
	if (bIsDead) return;

	if (bIsSkill2Cooldown)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, TEXT("Charge skill is on cooldown."));
		return;
	}

	Server_Skill2();
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

		if (Row->Cooldown > 0.0f)
		{
			Skill2CooldownTime = Row->Cooldown;
		}

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

		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Cyan, TEXT("Charge initiated."));
	}
}

void ATitanCharacter::OnChargeOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) return;

	if (!bIsCharging || OtherActor == this || HitVictims.Contains(OtherActor)) return;

	if (ACharacter* VictimChar = Cast<ACharacter>(OtherActor))
	{
		GetCapsuleComponent()->IgnoreActorWhenMoving(VictimChar, true);
		VictimChar->GetCapsuleComponent()->IgnoreActorWhenMoving(this, true);

		HitVictims.Add(VictimChar);

		UGameplayStatics::ApplyDamage(OtherActor, CurrentSkillDamage, GetController(), this, UDamageType::StaticClass());

		bool bIsBase = OtherActor->IsA(ABaseCharacter::StaticClass());
		bool bIsNormalEnemy = OtherActor->IsA(AEnemyNormal::StaticClass());

		if (bIsBase || bIsNormalEnemy)
		{
			FVector KnockbackDir = GetActorForwardVector();
			FVector LaunchForce = (KnockbackDir * 500.f) + FVector(0, 0, 1000.f);
			VictimChar->LaunchCharacter(LaunchForce, true, true);
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

void ATitanCharacter::ResetSkill2Cooldown()
{
	bIsSkill2Cooldown = false;
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Green, TEXT("Charge skill ready."));
}

void ATitanCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATitanCharacter, GrabbedActor);
}

void ATitanCharacter::OnChargeHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority()) return;
	if (!bIsCharging) return;

	if (OtherActor->IsRootComponentStatic())
	{
		StopCharge();
	}
}