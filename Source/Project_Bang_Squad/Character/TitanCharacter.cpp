#include "Project_Bang_Squad/Character/TitanCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h" 
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h" 
#include "TimerManager.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Net/UnrealNetwork.h" 
#include "Project_Bang_Squad/Character/Enemy/EnemyNormal.h"

ATitanCharacter::ATitanCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	GetCharacterMovement()->MaxWalkSpeed = 450.f;

	// [필수] 네트워크 복제 활성화
	bReplicates = true;
	SetReplicateMovement(true);

	GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &ATitanCharacter::OnChargeHit);
}

void ATitanCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATitanCharacter, bIsGrabbing);
	DOREPLIFETIME(ATitanCharacter, GrabbedActor);
}

void ATitanCharacter::BeginPlay()
{
	Super::BeginPlay();
	GetCapsuleComponent()->OnComponentBeginOverlap.AddDynamic(this, &ATitanCharacter::OnChargeOverlap);
}

void ATitanCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 로컬 플레이어 시각 효과 (하이라이트)
	if (!bIsGrabbing && IsLocallyControlled())
	{
		UpdateHoverHighlight();
	}
}

// =============================================================
// [기본 공격] Attack
// =============================================================
void ATitanCharacter::Attack()
{
	ProcessSkill(TEXT("Attack"));
}

// =============================================================
// [직업 능력] 잡기 & 던지기 (Job Ability)
// =============================================================
void ATitanCharacter::JobAbility()
{
	// 쿨타임 체크
	if (bIsCooldown)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, TEXT("Grab ability is on cooldown."));
		return;
	}

	if (!bIsGrabbing)
	{
		// [클라] 잡기 요청
		if (HoveredActor)
		{
			Server_TryGrab(HoveredActor);
		}
		else
		{
			// 타겟이 없을 때 알림
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Silver, TEXT("No target to grab."));
		}
	}
	else
	{
		// [클라] 던지기 요청
		FVector ThrowDir = GetControlRotation().Vector();
		Server_ThrowTarget(ThrowDir);
	}
}

// [서버] 잡기 실행
void ATitanCharacter::Server_TryGrab_Implementation(AActor* TargetToGrab)
{
	if (!TargetToGrab) return;

	// 플레이어(Base)이거나 에너미 노말(EnemyNormal)인지 확인
	bool bIsBase = TargetToGrab->IsA(ABaseCharacter::StaticClass());
	bool bIsNormalEnemy = TargetToGrab->IsA(AEnemyNormal::StaticClass());

	// 해당되는 대상만 잡기 시도
	if (bIsBase || bIsNormalEnemy)
	{
		GrabbedActor = TargetToGrab;
		bIsGrabbing = true;

		ProcessSkill(TEXT("JobAbility")); // 애니메이션 재생

		// 물리 제어는 공통 부모인 ACharacter로 캐스팅
		if (ACharacter* VictimChar = Cast<ACharacter>(GrabbedActor))
		{
			// 1. 공통 물리/충돌 끄기
			VictimChar->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			VictimChar->GetMesh()->SetSimulatePhysics(false);

			// [수정] IgnoreActorWhenMoving 사용 (true = 무시 켜기)
			GetCapsuleComponent()->IgnoreActorWhenMoving(VictimChar, true);
			VictimChar->GetCapsuleComponent()->IgnoreActorWhenMoving(this, true);

			// 손에 부착
			VictimChar->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Hand_R_Socket"));

			// 2. 플레이어(Base) 전용 처리
			if (ABaseCharacter* BaseVictim = Cast<ABaseCharacter>(VictimChar))
			{
				BaseVictim->SetIsGrabbed(true);
			}

			// 3. 에너미(EnemyNormal) 전용 처리
			if (AEnemyNormal* EnemyVictim = Cast<AEnemyNormal>(VictimChar))
			{
				EnemyVictim->GetCharacterMovement()->StopMovementImmediately();
				EnemyVictim->GetCharacterMovement()->DisableMovement();
			}

			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, TEXT("Target grabbed."));
		}

		// 5초 뒤 자동 던지기 예약
		FTimerDelegate TimerDel;
		TimerDel.BindUFunction(this, FName("Server_ThrowTarget"), GetActorForwardVector());
		GetWorldTimerManager().SetTimer(GrabTimerHandle, TimerDel, GrabMaxDuration, false);
	}
	else
	{
		// 잡을 수 없는 대상 (보스 등)
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("Target is immune to grabbing."));
	}
}

// [서버] 던지기 실행
void ATitanCharacter::Server_ThrowTarget_Implementation(FVector ThrowDirection)
{
	if (!bIsGrabbing || !GrabbedActor) return;

	GetWorldTimerManager().ClearTimer(GrabTimerHandle);
	GrabbedActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	if (ACharacter* VictimChar = Cast<ACharacter>(GrabbedActor))
	{
		// 1. 충돌 및 이동 복구
		VictimChar->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		VictimChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

		VictimChar->GetCharacterMovement()->SetMovementMode(MOVE_Falling);

		// 에너미 노말이면 DisableMovement를 풀어줘야 함
		if (VictimChar->IsA(AEnemyNormal::StaticClass()))
		{
			VictimChar->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			VictimChar->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		}

		// 2. 발사 속도 계산
		FVector LaunchVelocity = ThrowDirection * 2200.0f;
		LaunchVelocity += FVector(0, 0, 800.0f); // 상향 보정

		// 3. 발사
		VictimChar->LaunchCharacter(LaunchVelocity, true, true);

		// 4. 물리/상태 설정
		VictimChar->GetCharacterMovement()->GravityScale = 4.5f;
		VictimChar->GetCharacterMovement()->AirControl = 0.0f;

		// 베이스 캐릭터라면 던진 사람 기록
		if (ABaseCharacter* BaseVictim = Cast<ABaseCharacter>(VictimChar))
		{
			BaseVictim->SetThrownByTitan(true, this);
			BaseVictim->SetIsGrabbed(false);
		}

		// [수정] IgnoreActorWhenMoving 사용 (false = 무시 끄기/해제)
		GetCapsuleComponent()->IgnoreActorWhenMoving(VictimChar, false);
		VictimChar->GetCapsuleComponent()->IgnoreActorWhenMoving(this, false);

		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, TEXT("Target thrown."));
	}

	bIsGrabbing = false;
	GrabbedActor = nullptr;

	// 쿨타임 시작
	bIsCooldown = true;
	GetWorldTimerManager().SetTimer(CooldownTimerHandle, this, &ATitanCharacter::ResetCooldown, ThrowCooldownTime, false);
}

// =============================================================
// [스킬 2] 돌진 (Charge)
// =============================================================
void ATitanCharacter::Skill2()
{
	if (bIsSkill2Cooldown)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, TEXT("Charge skill is on cooldown."));
		return;
	}

	Server_Skill2();
}

// [서버] 돌진 실행
void ATitanCharacter::Server_Skill2_Implementation()
{
	// 데이터 테이블 확인
	if (!SkillDataTable) return;
	static const FString ContextString(TEXT("Skill2 Context"));
	FSkillData* Row = SkillDataTable->FindRow<FSkillData>(TEXT("Skill2"), ContextString);

	if (Row)
	{
		if (!IsSkillUnlocked(Row->RequiredStage)) return;
		CurrentSkillDamage = Row->Damage;
		if (Row->SkillMontage) PlayAnimMontage(Row->SkillMontage);
	}

	if (!bIsCharging)
	{
		bIsCharging = true;
		HitVictims.Empty();

		// 물리 설정 저장
		DefaultGroundFriction = GetCharacterMovement()->GroundFriction;
		DefaultGravityScale = GetCharacterMovement()->GravityScale;

		// 물리 끄기 (직선 이동)
		GetCharacterMovement()->GroundFriction = 0.0f;
		GetCharacterMovement()->GravityScale = 0.0f;
		GetCharacterMovement()->BrakingDecelerationFlying = 0.0f;

		// 충돌 무시 (Overlap)
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);

		// 발사
		FVector LaunchDir = GetActorForwardVector();
		LaunchCharacter(LaunchDir * 3000.f, true, true);

		// 쿨타임 및 정지 타이머
		bIsSkill2Cooldown = true;
		GetWorldTimerManager().SetTimer(Skill2CooldownTimerHandle, this, &ATitanCharacter::ResetSkill2Cooldown, Skill2CooldownTime, false);
		GetWorldTimerManager().SetTimer(ChargeTimerHandle, this, &ATitanCharacter::StopCharge, 0.3f, false);

		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Cyan, TEXT("Charge initiated."));
	}
}

void ATitanCharacter::OnChargeOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// [서버 권한] 데미지 처리
	if (!HasAuthority()) return;

	if (!bIsCharging || OtherActor == this || HitVictims.Contains(OtherActor)) return;

	if (ACharacter* VictimChar = Cast<ACharacter>(OtherActor))
	{
		// [수정] IgnoreActorWhenMoving 사용 (true = 무시 켜기)
		GetCapsuleComponent()->IgnoreActorWhenMoving(VictimChar, true);
		VictimChar->GetCapsuleComponent()->IgnoreActorWhenMoving(this, true);

		HitVictims.Add(VictimChar);

		// 1. 데미지는 누구든지 적용
		UGameplayStatics::ApplyDamage(OtherActor, CurrentSkillDamage, GetController(), this, UDamageType::StaticClass());

		// 2. 넉백(날리기)은 'BaseCharacter' 또는 'EnemyNormal' 일 때만!
		bool bIsBase = OtherActor->IsA(ABaseCharacter::StaticClass());
		bool bIsNormalEnemy = OtherActor->IsA(AEnemyNormal::StaticClass());

		if (bIsBase || bIsNormalEnemy)
		{
			FVector KnockbackDir = GetActorForwardVector();
			FVector LaunchForce = (KnockbackDir * 500.f) + FVector(0, 0, 1000.f);
			VictimChar->LaunchCharacter(LaunchForce, true, true);

			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Red, TEXT("Target impacted."));
		}
		else
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Orange, TEXT("Target is too heavy! Damage only."));
		}
	}
}

void ATitanCharacter::StopCharge()
{
	bIsCharging = false;
	GetCharacterMovement()->StopMovementImmediately();

	// 물리 복구
	GetCharacterMovement()->GroundFriction = DefaultGroundFriction;
	GetCharacterMovement()->GravityScale = DefaultGravityScale;

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);

	for (AActor* IgnoredActor : HitVictims)
	{
		// [수정] IgnoreActorWhenMoving 사용 (false = 무시 끄기)
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

	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Silver, TEXT("Charge stopped."));
}

void ATitanCharacter::ResetSkill2Cooldown()
{
	bIsSkill2Cooldown = false;
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Green, TEXT("Charge skill ready."));
}

void ATitanCharacter::OnChargeHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority()) return;

	if (!bIsCharging) return;

	if (OtherActor->IsRootComponentStatic())
	{
		StopCharge();
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Orange, TEXT("Collision with obstacle. Stopping charge."));
	}
}

// =============================================================
// [스킬 1]
// =============================================================
void ATitanCharacter::Skill1()
{
	ProcessSkill(TEXT("Skill1"));
}

// =============================================================
// 유틸리티 함수들
// =============================================================

void ATitanCharacter::ProcessSkill(FName SkillRowName)
{
	if (!SkillDataTable)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Error: Skill Data Table is missing."));
		return;
	}

	static const FString ContextString(TEXT("TitanSkillContext"));
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
	else
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
			FString::Printf(TEXT("Error: Skill data '%s' not found."), *SkillRowName.ToString()));
	}
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

void ATitanCharacter::ResetCooldown()
{
	bIsCooldown = false;
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Green, TEXT("Grab ability is ready."));
}