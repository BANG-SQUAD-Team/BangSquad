#include "Project_Bang_Squad/Character/TitanCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h" 
#include "TimerManager.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h" // [필수] GEngine 사용을 위해 꼭 추가해야 함!

ATitanCharacter::ATitanCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	GetCharacterMovement()->MaxWalkSpeed = 450.f;

	// [중요] 캡슐 컴포넌트의 충돌(Hit) 이벤트를 감지하여 돌진 데미지를 줌
	GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &ATitanCharacter::OnChargeHit);
}

void ATitanCharacter::BeginPlay()
{
	Super::BeginPlay();

	// [필수] 오버랩(겹침) 이벤트 연결
	GetCapsuleComponent()->OnComponentBeginOverlap.AddDynamic(this, &ATitanCharacter::OnChargeOverlap);
}

void ATitanCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bIsGrabbing) { UpdateHoverHighlight(); }
}

// =============================================================
// [기본 공격] Attack
// =============================================================
void ATitanCharacter::Attack()
{
	// 데이터 테이블에 "Attack" 행이 있어야 하며, RequiredStage가 0이어야 함
	ProcessSkill(TEXT("Attack"));
}

void ATitanCharacter::Skill2()
{
	// 1. 쿨타임 체크
	if (bIsSkill2Cooldown)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, TEXT("Skill2 Cooldown..."));
		return;
	}

	// 2. 데이터 테이블 파싱 (데미지, 몽타주)
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

		// 1. 물리 설정 저장
		DefaultGroundFriction = GetCharacterMovement()->GroundFriction;
		DefaultGravityScale = GetCharacterMovement()->GravityScale;

		// 2. 물리 끄기 (직선 비행 모드)
		GetCharacterMovement()->GroundFriction = 0.0f;
		GetCharacterMovement()->GravityScale = 0.0f;

		// [중요] 비행 중 감속 끄기 (공기 저항 0)
		GetCharacterMovement()->BrakingDecelerationFlying = 0.0f;

		// =================================================================
		// [강력한 해결책] 움직이는 모든 것(적 몸체, 무기, 소환수 등)을 무시(Overlap)
		// =================================================================
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap); // 적 메시가 보통 이거임
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);  // 물리 물체도 무시

		// 3. 발사 (속도 5000, Z축 무시하고 정면으로)
		FVector LaunchDir = GetActorForwardVector();
		LaunchCharacter(LaunchDir * 3000.f, true, true);

		// 4. 타이머 설정 (1.2초)
		bIsSkill2Cooldown = true;
		GetWorldTimerManager().SetTimer(Skill2CooldownTimerHandle, this, &ATitanCharacter::ResetSkill2Cooldown, Skill2CooldownTime, false);
		GetWorldTimerManager().SetTimer(ChargeTimerHandle, this, &ATitanCharacter::StopCharge, 0.3f, false); // 시간 조절 가능

		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Cyan, TEXT("🚄 전차 모드 가동! 궤적 고정!"));
	}
}

void ATitanCharacter::OnChargeOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 나 자신이거나 / 이미 때린 적이면 무시
	if (!bIsCharging || OtherActor == this || HitVictims.Contains(OtherActor)) return;

	// ACharacter 뿐만 아니라 움직이는 폰(Pawn) 계열은 다 날려버림
	if (OtherActor->IsA(APawn::StaticClass()))
	{
		ACharacter* Victim = Cast<ACharacter>(OtherActor);

		// [중요] 엔진 레벨에서 "이 둘은 서로 물리 충돌 계산하지 마"라고 확정 지음
		// (혹시라도 캡슐 외에 다른 게 부딪힐 가능성 차단)
		this->MoveIgnoreActorAdd(OtherActor);
		if (Victim) Victim->MoveIgnoreActorAdd(this);

		// 1. 목록 등록
		HitVictims.Add(OtherActor);

		// 2. 데미지
		UGameplayStatics::ApplyDamage(OtherActor, CurrentSkillDamage, GetController(), this, UDamageType::StaticClass());

		// 3. 넉백 (적은 내 진행방향 대각선 위로 날아감)
		if (Victim)
		{
			FVector KnockbackDir = GetActorForwardVector();
			FVector LaunchForce = (KnockbackDir * 500.f) + FVector(0, 0, 1000.f); // 좀 더 세게 날림
			Victim->LaunchCharacter(LaunchForce, true, true);
		}

		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Red, TEXT("💨 비켜!!"));
	}
}

void ATitanCharacter::StopCharge()
{
	bIsCharging = false;
	GetCharacterMovement()->StopMovementImmediately();

	// 1. 물리 복구
	GetCharacterMovement()->GroundFriction = DefaultGroundFriction;
	GetCharacterMovement()->GravityScale = DefaultGravityScale;
	// GetCharacterMovement()->BrakingDecelerationFlying = (원래값, 보통 0이 아닐 수 있음);

	// =================================================================
	// [복구] 다시 모든 물체와 충돌하도록(Block) 설정
	// =================================================================
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block); // 필요하면 Block or Overlap

	// [중요] MoveIgnore 했던 적들 목록 초기화 (다시 부딪힐 수 있게)
	// HitVictims에 있는 애들 다시 인식하게 해줌
	for (AActor* IgnoredActor : HitVictims)
	{
		if (IgnoredActor) this->MoveIgnoreActorRemove(IgnoredActor);
	}
	HitVictims.Empty();

	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Silver, TEXT("🛑 정지 완료"));
}

void ATitanCharacter::ResetSkill2Cooldown()
{
	bIsSkill2Cooldown = false;
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Green, TEXT("Skill2 Ready!"));
}

void ATitanCharacter::OnChargeHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 돌진 중이 아니면 무시
	if (!bIsCharging) return;

	// 벽이나 바닥 같은 정적 물체(Static)에 박으면?
	if (OtherActor->IsRootComponentStatic())
	{

		// 벽에 쾅! 박으면 멈춤
		StopCharge();
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Orange, TEXT("🧱 벽 충돌! 멈춤"));
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
// [직업 능력] 잡기 & 던지기 (Job Ability)
// =============================================================
void ATitanCharacter::JobAbility()
{
	if (bIsCooldown)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, TEXT("Ability on Cooldown..."));
		return;
	}

	if (!bIsGrabbing) { TryGrab(); }
	else { ThrowTarget(); }
}

void ATitanCharacter::TryGrab()
{
	if (!HoveredActor) return;

	// 보스급은 못 잡음 (태그 확인)
	if (HoveredActor->ActorHasTag("Boss") || HoveredActor->ActorHasTag("MidBoss"))
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("Too heavy to grab!"));
		return;
	}

	GrabbedActor = HoveredActor;
	HoveredActor = nullptr;
	bIsGrabbing = true;

	SetHighlight(GrabbedActor, true);
	ProcessSkill(TEXT("JobAbility")); // 몽타주 재생

	if (ABaseCharacter* Victim = Cast<ABaseCharacter>(GrabbedActor))
	{

		Victim->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Victim->GetMesh()->SetSimulatePhysics(false); // 물리 끄기


		// 2. 잡힘 상태 알림
		Victim->SetIsGrabbed(true);


		this->MoveIgnoreActorAdd(Victim);
		Victim->MoveIgnoreActorAdd(this);
	}

	// 4. 소켓에 부착
	GrabbedActor->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Hand_R_Socket"));

	// 5초 뒤 자동 던지기
	GetWorldTimerManager().SetTimer(GrabTimerHandle, this, &ATitanCharacter::ThrowTarget, GrabMaxDuration, false);
}

void ATitanCharacter::ThrowTarget()
{
	if (!bIsGrabbing || !GrabbedActor) return;

	GetWorldTimerManager().ClearTimer(GrabTimerHandle);
	SetHighlight(GrabbedActor, false);
	GrabbedActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	if (ABaseCharacter* Victim = Cast<ABaseCharacter>(GrabbedActor))
	{
		// 1. 충돌 및 이동 모드 복구
		Victim->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Victim->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		Victim->GetCharacterMovement()->SetMovementMode(MOVE_Falling);

		// 2. 던질 궤적 계산 (신드라 W 스타일 포물선)
		FVector StartPos = Victim->GetActorLocation();
		FVector ForwardVector = GetControlRotation().Vector();
		FVector TargetPos = StartPos + (ForwardVector * 2200.0f); // 2200 = 비거리

		FVector LaunchVelocity;
		// 0.85 = 높은 포물선 (산 모양)
		bool bHaveSolution = UGameplayStatics::SuggestProjectileVelocity_CustomArc(
			this, LaunchVelocity, StartPos, TargetPos, 0.0f, 0.85f);

		if (!bHaveSolution)
		{
			// 실패 시 직사 보정
			LaunchVelocity = ForwardVector * 1500.0f + FVector(0, 0, 800.0f);
		}

		// 3. 발사 (Launch!)
		Victim->LaunchCharacter(LaunchVelocity, true, true);

		// 4. 던져진 상태 설정 (중력 증가로 묵직하게 꽂힘)
		Victim->SetThrownByTitan(true, this);
		Victim->GetCharacterMovement()->GravityScale = 4.5f;
		Victim->GetCharacterMovement()->AirControl = 0.0f;

		// 잡힘 해제
		Victim->SetIsGrabbed(false);
		this->MoveIgnoreActorRemove(Victim);
		Victim->MoveIgnoreActorRemove(this);
	}

	bIsGrabbing = false;
	GrabbedActor = nullptr;
	bIsCooldown = true;
	GetWorldTimerManager().SetTimer(CooldownTimerHandle, this, &ATitanCharacter::ResetCooldown, ThrowCooldownTime, false);
}

// =============================================================
// 유틸리티 함수들
// =============================================================

void ATitanCharacter::ProcessSkill(FName SkillRowName)
{
	if (!SkillDataTable)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("❌ [오류] Skill Data Table이 비어있음!"));
		return;
	}

	static const FString ContextString(TEXT("TitanSkillContext"));
	FSkillData* Data = SkillDataTable->FindRow<FSkillData>(SkillRowName, ContextString);

	if (Data)
	{
		if (!IsSkillUnlocked(Data->RequiredStage))
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Orange, TEXT("🔒 스킬 잠김"));
			return;
		}

		// 몽타주 재생
		if (Data->SkillMontage)
		{
			PlayAnimMontage(Data->SkillMontage);
		}
	}
	else
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
			FString::Printf(TEXT("❌ [오류] 데이터 테이블에 '%s' 가 없음!"), *SkillRowName.ToString()));
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
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Green, TEXT("Job Ability Ready!"));
}