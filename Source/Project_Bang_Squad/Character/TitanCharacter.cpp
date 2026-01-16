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
	bReplicates = true; // 서버 복제 활성화
}

// =================================================================
// [생명주기 및 오버라이드]
// =================================================================

void ATitanCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 돌진 충돌 감지 바인딩
	GetCapsuleComponent()->OnComponentBeginOverlap.AddDynamic(this, &ATitanCharacter::OnChargeOverlap);
	GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &ATitanCharacter::OnChargeHit);
}

void ATitanCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 내가 조종하는 로컬 플레이어이고, 잡는 중이 아니라면 하이라이트 갱신
	if (IsLocallyControlled() && !bIsGrabbing)
	{
		UpdateHoverHighlight();
	}
}

void ATitanCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// GrabbedActor 변수가 바뀌면 클라이언트로 전송
	DOREPLIFETIME(ATitanCharacter, GrabbedActor);
}

void ATitanCharacter::OnDeath()
{
	if (bIsDead) return;

	// 죽으면 잡고 있던 것 놓기 (서버 권한 확인)
	if (HasAuthority() && bIsGrabbing && GrabbedActor)
	{
		Server_ThrowTarget();
	}

	// 돌진 중지
	if (bIsCharging)
	{
		StopCharge();
	}

	// 하이라이트 해제
	if (HoveredActor)
	{
		SetHighlight(HoveredActor, false);
		HoveredActor = nullptr;
	}

	// 타이머 정리
	GetWorldTimerManager().ClearTimer(GrabTimerHandle);
	GetWorldTimerManager().ClearTimer(ChargeTimerHandle);
	GetWorldTimerManager().ClearTimer(CooldownTimerHandle);
	GetWorldTimerManager().ClearTimer(Skill2CooldownTimerHandle);
	GetWorldTimerManager().ClearTimer(Skill1CooldownTimerHandle);
	GetWorldTimerManager().ClearTimer(RockThrowTimerHandle);

	StopAnimMontage();
	Super::OnDeath();
}

// =================================================================
// [입력 핸들러] (플레이어 입력 -> 서버 요청)
// =================================================================

void ATitanCharacter::Attack()
{
	if (!CanAttack()) return;

	// A/B 공격 모션 결정
	FName SkillRowName = bIsNextAttackA ? TEXT("Attack_A") : TEXT("Attack_B");

	// 서버에 공격 요청
	Server_Attack(SkillRowName);

	bIsNextAttackA = !bIsNextAttackA;
}

void ATitanCharacter::JobAbility()
{
	if (bIsDead || bIsCooldown) return;

	if (!bIsGrabbing)
	{
		// 잡기 시도: 하이라이트된 대상이 있으면 서버에 요청
		if (HoveredActor)
		{
			Server_TryGrab(HoveredActor);
		}
	}
	else
	{
		// 던지기 시도
		Server_ThrowTarget();
	}
}

void ATitanCharacter::Skill1()
{
	if (bIsDead || bIsSkill1Cooldown) return;

	// 서버에 바위 던지기 요청
	Server_Skill1();
}

void ATitanCharacter::Skill2()
{
	if (bIsDead || bIsSkill2Cooldown) return;

	// 서버에 돌진 요청
	Server_Skill2();
}

void ATitanCharacter::ExecuteGrab()
{
	// 몽타주 노티파이용 (실제 로직은 Server_TryGrab에서 처리됨)
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Magenta, TEXT("ExecuteGrab Called (Visual Only)"));
}

// =================================================================
// [네트워크 구현: 평타]
// =================================================================

void ATitanCharacter::Server_Attack_Implementation(FName SkillName)
{
	// 쿨타임 데이터 처리 (서버)
	if (SkillDataTable)
	{
		static const FString ContextString(TEXT("TitanAttack"));
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

void ATitanCharacter::Multicast_Attack_Implementation(FName SkillName)
{
	// 모션 재생 (모든 클라이언트)
	ProcessSkill(SkillName);
}

// =================================================================
// [네트워크 구현: 직업 스킬 (잡기/던지기)]
// =================================================================

void ATitanCharacter::Server_TryGrab_Implementation(AActor* TargetToGrab)
{
	if (!TargetToGrab || bIsGrabbing) return;

	// 거리 검증
	float DistSq = FVector::DistSquared(GetActorLocation(), TargetToGrab->GetActorLocation());
	if (DistSq > 1000.f * 1000.f) return;

	// 1. 변수 업데이트 (Replicated)
	GrabbedActor = TargetToGrab;
	bIsGrabbing = true;

	// 2. 서버에서 부착 수행
	GrabbedActor->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Hand_R_Socket"));

	// 3. 물리 상태 제어
	if (ACharacter* Victim = Cast<ACharacter>(GrabbedActor))
	{
		SetHeldState(Victim, true);
	}
	else if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(GrabbedActor->GetRootComponent()))
	{
		RootComp->SetSimulatePhysics(false);
	}

	// 4. 모션 및 타이머
	Multicast_PlayJobMontage(TEXT("Grab"));
	GetWorldTimerManager().SetTimer(GrabTimerHandle, this, &ATitanCharacter::Server_ThrowTarget, GrabMaxDuration, false);
}

void ATitanCharacter::OnRep_GrabbedActor()
{
	// 서버에서 GrabbedActor 변수가 변경되면 모든 클라이언트에서 실행됨
	if (GrabbedActor)
	{
		// [잡혔을 때] 동기화
		bIsGrabbing = true;

		// 물리/이동 끄기
		if (ACharacter* Victim = Cast<ACharacter>(GrabbedActor))
		{
			SetHeldState(Victim, true);
		}
		else if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(GrabbedActor->GetRootComponent()))
		{
			RootComp->SetSimulatePhysics(false);
		}

		// 시각적 부착 (클라이언트 보정)
		GrabbedActor->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Hand_R_Socket"));
	}
	else
	{
		// [놓아줬을 때] 동기화
		bIsGrabbing = false;
		// 상태 복구는 던지기 로직(LaunchCharacter 등)에 의해 처리됨
	}
}

void ATitanCharacter::Multicast_PlayJobMontage_Implementation(FName SectionName)
{
	// 모든 클라이언트에서 JobAbility의 특정 섹션(Grab/Throw) 재생
	ProcessSkill(TEXT("JobAbility"), SectionName);
}

void ATitanCharacter::Server_ThrowTarget_Implementation()
{
	if (!bIsGrabbing || !GrabbedActor) return;

	GetWorldTimerManager().ClearTimer(GrabTimerHandle);

	// 쿨타임 설정
	if (SkillDataTable)
	{
		static const FString ContextString(TEXT("TitanThrowContext"));
		FSkillData* Row = SkillDataTable->FindRow<FSkillData>(TEXT("JobAbility"), ContextString);
		if (Row && Row->Cooldown > 0.0f) ThrowCooldownTime = Row->Cooldown;
	}

	Multicast_PlayJobMontage(TEXT("Throw"));

	// 부착 해제
	GrabbedActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// 회전값 초기화
	FRotator ActorRot = GrabbedActor->GetActorRotation();
	GrabbedActor->SetActorRotation(FRotator(0.f, ActorRot.Yaw, 0.f));
	GrabbedActor->AddActorWorldOffset(FVector(0.f, 0.f, 50.f), false, nullptr, ETeleportType::TeleportPhysics);

	// 물리력 가하기 (LaunchCharacter는 서버 호출 시 동기화됨)
	if (ACharacter* Victim = Cast<ACharacter>(GrabbedActor))
	{
		SetHeldState(Victim, false);

		FVector ThrowDir = GetControlRotation().Vector();
		ThrowDir = (ThrowDir + FVector(0.f, 0.f, 0.25f)).GetSafeNormal();

		Victim->LaunchCharacter(ThrowDir * ThrowForce, true, true);

		// 복구 타이머
		FTimerHandle RecoveryHandle;
		FTimerDelegate Delegate;
		Delegate.BindUFunction(this, TEXT("RecoverCharacter"), Victim);
		GetWorldTimerManager().SetTimer(RecoveryHandle, Delegate, 1.5f, false);
	}
	else if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(GrabbedActor->GetRootComponent()))
	{
		RootComp->SetSimulatePhysics(true);
		RootComp->AddImpulse(GetControlRotation().Vector() * ThrowForce * 50.f);
	}

	// 상태 초기화 (OnRep 트리거)
	GrabbedActor = nullptr;
	bIsGrabbing = false;

	bIsCooldown = true;
	GetWorldTimerManager().SetTimer(CooldownTimerHandle, this, &ATitanCharacter::ResetCooldown, ThrowCooldownTime, false);
}

// =================================================================
// [네트워크 구현: 스킬 1 (바위)]
// =================================================================

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
		if (Row->SkillMontage) PlayAnimMontage(Row->SkillMontage); // 서버에서 재생 시 클라 복제됨
	}

	// 바위 생성 (서버에서 스폰 -> 자동 리플리케이션)
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

	// 일정 시간 후 던지기
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

// =================================================================
// [네트워크 구현: 스킬 2 (돌진)]
// =================================================================

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

		// 마찰력/중력 제거 (서버)
		DefaultGroundFriction = GetCharacterMovement()->GroundFriction;
		DefaultGravityScale = GetCharacterMovement()->GravityScale;
		GetCharacterMovement()->GroundFriction = 0.0f;
		GetCharacterMovement()->GravityScale = 0.0f;
		GetCharacterMovement()->BrakingDecelerationFlying = 0.0f;

		// 충돌 설정
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);

		// 발사 (서버에서 실행 -> 동기화됨)
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
		GetCapsuleComponent()->IgnoreActorWhenMoving(VictimChar, true);
		VictimChar->GetCapsuleComponent()->IgnoreActorWhenMoving(this, true);
		HitVictims.Add(VictimChar);

		UGameplayStatics::ApplyDamage(OtherActor, CurrentSkillDamage, GetController(), this, UDamageType::StaticClass());

		// 넉백 적용
		FVector KnockbackDir = GetActorForwardVector();
		FVector LaunchForce = (KnockbackDir * 500.f) + FVector(0, 0, 1000.f);
		VictimChar->LaunchCharacter(LaunchForce, true, true);
	}
}

void ATitanCharacter::OnChargeHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority() || !bIsCharging) return;
	// 벽 등에 부딪히면 멈춤
	if (OtherActor->IsRootComponentStatic())
	{
		StopCharge();
	}
}

// =================================================================
// [헬퍼 및 유틸리티 함수]
// =================================================================

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
		// 보스나 미드보스가 아니면 잡기 가능
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
	Victim->SetActorRotation(FRotator(0.f, CurrentRot.Yaw, 0.f)); // 기울어짐 복구

	if (Victim->GetCapsuleComponent())
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

void ATitanCharacter::ResetCooldown() { bIsCooldown = false; }
void ATitanCharacter::ResetSkill1Cooldown() { bIsSkill1Cooldown = false; }
void ATitanCharacter::ResetSkill2Cooldown() { bIsSkill2Cooldown = false; }