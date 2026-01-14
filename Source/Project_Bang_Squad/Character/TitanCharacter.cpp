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
#include "Net/UnrealNetwork.h"

ATitanCharacter::ATitanCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ATitanCharacter::BeginPlay()
{
	Super::BeginPlay();

	// [핵심 수정 사항] 캡슐 컴포넌트의 오버랩(겹침) 이벤트에 함수를 등록합니다.
	GetCapsuleComponent()->OnComponentBeginOverlap.AddDynamic(this, &ATitanCharacter::OnChargeOverlap);

	// [추가] 벽 충돌(Hit) 함수도 만들어 두셨으니, 이것도 등록해야 벽에 박았을 때 멈춥니다.
	GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &ATitanCharacter::OnChargeHit);
}

void ATitanCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bIsGrabbing) { UpdateHoverHighlight(); }
}

void ATitanCharacter::OnDeath()
{
	// 이미 죽었으면 무시
	if (bIsDead) return;
	
	// [잡기 시스템 정리] 죽는 순간 잡고 있던 놈을 놔줘야 함
	if (bIsGrabbing && GrabbedActor)
	{
		// 손에서 떼어내기
		GrabbedActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		
		// 잡혀 있던 사람의 물리/AI 상태 복구
		if (ACharacter* Victim = Cast<ACharacter>(GrabbedActor))
		{
			SetHeldState(Victim, false);
			
			// 땅으로 떨어지게 무브먼트 모드 변경
			if (Victim->GetCharacterMovement())
			{
				Victim->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			}
		}
		else if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(GrabbedActor))
		{
			// 사물이라면 물리 다시 켜주기
			RootComp->SetSimulatePhysics(true);
		}
		
		GrabbedActor = nullptr;
		bIsGrabbing = false;
		GetWorldTimerManager().ClearTimer(GrabTimerHandle);
	}
	
	// [돌진 시스템 정리] 돌진 중에 죽으면 미끄러짐 방지
	if (bIsCharging)
	{
		// StopCharge를 호출해서 마찰력 중력 콜리전 설정을 원래대로 되돌려놓음
		StopCharge();
		GetWorldTimerManager().ClearTimer(ChargeTimerHandle);
	}
	
	// 하이라이트 끄기
	if (HoveredActor)
	{
		SetHighlight(HoveredActor, false);
		HoveredActor = nullptr;
	}
	
	// 타이머들 정리
	GetWorldTimerManager().ClearTimer(CooldownTimerHandle);
	GetWorldTimerManager().ClearTimer(Skill2CooldownTimerHandle);
	
	// 몽타주 정지
	StopAnimMontage();
	
	// 부모 클래스 사망 로직
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
				// 이미 재생 중이면 해당 섹션으로 점프 (Hold -> Throw 전환 시 부드럽게)
				if (AnimInstance->Montage_IsPlaying(Data->SkillMontage))
				{
					if (StartSectionName != NAME_None)
					{
						AnimInstance->Montage_JumpToSection(StartSectionName, Data->SkillMontage);
					}
				}
				// 재생 중이 아니면 처음부터(혹은 지정 섹션부터) 재생
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
	if (!HoveredActor) return; // 대상 없으면 취소

	GrabbedActor = HoveredActor;
	HoveredActor = nullptr;
	bIsGrabbing = true;

	SetHighlight(GrabbedActor, true);

	// 몽타주 재생 (Grab 섹션)
	ProcessSkill(TEXT("JobAbility"), TEXT("Grab"));

	// 주의: 여기서 Attach(붙이기) 하던 코드는 다 지우거나 아래 ExecuteGrab으로 옮김!
}

// 2. ExecuteGrab: 노티파이가 딱! 호출하면 그때 붙입니다.
void ATitanCharacter::ExecuteGrab()
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Magenta, TEXT("ExecuteGrab Called!"));

	// 타겟이 없거나 잡기 상태가 아니면 무시
	if (!GrabbedActor || !bIsGrabbing) return;

	// [수정] ACharacter면 SetHeldState로 확실하게 AI랑 물리를 끕니다.
	if (ACharacter* Victim = Cast<ACharacter>(GrabbedActor))
	{
		SetHeldState(Victim, true);
	}
	else
	{
		// 캐릭터가 아닌 물체라면 기존 방식대로 물리만 끔
		if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(GrabbedActor->GetRootComponent()))
		{
			RootComp->SetSimulatePhysics(false);
		}
	}

	// 손에 부착!
	GrabbedActor->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Hand_R_Socket"));

	// 자동 던지기 타이머 시작
	GetWorldTimerManager().SetTimer(GrabTimerHandle, this, &ATitanCharacter::ThrowTarget, GrabMaxDuration, false);
}

void ATitanCharacter::ThrowTarget()
{
	if (!bIsGrabbing || !GrabbedActor) return;

	GetWorldTimerManager().ClearTimer(GrabTimerHandle);

	ProcessSkill(TEXT("JobAbility"), TEXT("Throw"));
	SetHighlight(GrabbedActor, false);

	GrabbedActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// 캐릭터 기울어짐 방지 및 바닥 끼임 방지 (필수 수정)
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

void ATitanCharacter::JobAbility()
{
	// 죽었으면 불가
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
	// 죽었으면 불가
	if (bIsDead) return;
	// 데이터 테이블에 "NormalAttack" (또는 "Attack") 키로 평타 데이터가 설정되어 있어야 합니다.
	ProcessSkill(TEXT("Attack"));
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
			// 로그로 잡으려는 대상의 이름을 명확히 확인
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

	// 1. 혹시 던져지고 나서도 기울어져 있다면 강제로 다시 세움
	FRotator CurrentRot = Victim->GetActorRotation();
	if (!FMath::IsNearlyZero(CurrentRot.Pitch) || !FMath::IsNearlyZero(CurrentRot.Roll))
	{
		Victim->SetActorRotation(FRotator(0.f, CurrentRot.Yaw, 0.f));
	}

	// 2. 캡슐 콜리전 확실하게 켜기 (Physics 타입이어야 함)
	if (Victim->GetCapsuleComponent()->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics)
	{
		Victim->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	// 3. 움직임 모드 강제 전환 (Falling 상태에서 굳는 걸 방지)
	if (Victim->GetCharacterMovement())
	{
		// 만약 여전히 Falling이나 None 상태라면 Walking으로 강제 전환 시도
		if (Victim->GetCharacterMovement()->MovementMode == MOVE_None ||
			Victim->GetCharacterMovement()->MovementMode == MOVE_Falling)
		{
			Victim->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}

		// 속도 초기화 (미끄러짐 방지)
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

	if (bIsHeld) // 잡혔을 때
	{
		// 1. 공격 중이던 몽타주 강제 취소 (제일 중요: 칼질 멈춤)
		Target->StopAnimMontage();

		// 2. 물리/이동 봉인
		if (CMC)
		{
			CMC->StopMovementImmediately();
			CMC->DisableMovement();
		}

		// 3. 충돌 끄기 (타이탄과 비벼서 날아가는거 방지)
		if (Capsule) Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// 4. [핵심] AI 뇌 정지 (Behavior Tree 멈춤 -> 공격 시도 안 함)
		if (AIC && AIC->GetBrainComponent())
		{
			AIC->GetBrainComponent()->StopLogic(TEXT("Grabbed"));
			AIC->StopMovement();
		}
	}
	else // 풀려났을 때 (던져질 때)
	{
		// 1. 충돌 다시 켜기
		if (Capsule) Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		// 2. [핵심] 던져질 때는 Falling 모드여야 날아감 (Walking이면 제자리 멈춤)
		if (CMC) CMC->SetMovementMode(MOVE_Falling);

		// 3. AI 다시 작동
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
	// 죽었으면 불가
	if (bIsDead) return;
	ProcessSkill(TEXT("Skill1"));
}
void ATitanCharacter::Skill2()
{
	// 죽었으면 불가
	if (bIsDead) return;
	
	// 쿨타임이면 실행 안 함
	if (bIsSkill2Cooldown)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, TEXT("Charge skill is on cooldown."));
		return;
	}

	// 서버로 실행 요청 (멀티플레이어 대응)
	Server_Skill2();
}

// [서버] 돌진 실행
void ATitanCharacter::Server_Skill2_Implementation()
{
	// 1. 데이터 테이블에서 몽타주 가져오기
	if (!SkillDataTable) return;
	static const FString ContextString(TEXT("Skill2 Context"));
	FSkillData* Row = SkillDataTable->FindRow<FSkillData>(TEXT("Skill2"), ContextString);

	if (Row)
	{
		if (!IsSkillUnlocked(Row->RequiredStage)) return;
		CurrentSkillDamage = Row->Damage;
		if (Row->SkillMontage) PlayAnimMontage(Row->SkillMontage);
	}

	// 2. 이미 돌진 중이 아니면 물리 설정 변경 후 발사
	if (!bIsCharging)
	{
		bIsCharging = true;
		HitVictims.Empty(); // 타격 리스트 초기화

		// 물리 설정 저장 (끝나고 복구용)
		DefaultGroundFriction = GetCharacterMovement()->GroundFriction;
		DefaultGravityScale = GetCharacterMovement()->GravityScale;

		// 마찰력과 중력을 0으로 (미끄러지듯 날아가게)
		GetCharacterMovement()->GroundFriction = 0.0f;
		GetCharacterMovement()->GravityScale = 0.0f;
		GetCharacterMovement()->BrakingDecelerationFlying = 0.0f;

		// 폰(Pawn)끼리 충돌하면 멈추지 않고 뚫고 지나가면서 데미지(Overlap)를 주기 위함
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);

		// 3. 실제 발사 (Launch) - 보는 방향으로 3000 힘으로 발사
		FVector LaunchDir = GetActorForwardVector();
		LaunchCharacter(LaunchDir * 3000.f, true, true);

		// 4. 타이머 설정 (0.3초 뒤 정지, 쿨타임 시작)
		bIsSkill2Cooldown = true;
		GetWorldTimerManager().SetTimer(Skill2CooldownTimerHandle, this, &ATitanCharacter::ResetSkill2Cooldown, Skill2CooldownTime, false);
		GetWorldTimerManager().SetTimer(ChargeTimerHandle, this, &ATitanCharacter::StopCharge, 0.3f, false);

		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Cyan, TEXT("Charge initiated."));
	}
}

// [충돌 감지] 돌진 중에 누가 닿으면 호출됨
void ATitanCharacter::OnChargeOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) return; // 서버만 처리

	// 돌진 중이 아니거나, 자기 자신이거나, 이미 때린 놈이면 패스
	if (!bIsCharging || OtherActor == this || HitVictims.Contains(OtherActor)) return;

	if (ACharacter* VictimChar = Cast<ACharacter>(OtherActor))
	{
		// 충돌 시 서로 밀려나지 않게 무시 설정
		GetCapsuleComponent()->IgnoreActorWhenMoving(VictimChar, true);
		VictimChar->GetCapsuleComponent()->IgnoreActorWhenMoving(this, true);

		HitVictims.Add(VictimChar);

		// 1. 데미지 적용
		UGameplayStatics::ApplyDamage(OtherActor, CurrentSkillDamage, GetController(), this, UDamageType::StaticClass());

		// 2. 넉백 (BaseCharacter나 EnemyNormal만 날려버림)
		bool bIsBase = OtherActor->IsA(ABaseCharacter::StaticClass());
		bool bIsNormalEnemy = OtherActor->IsA(AEnemyNormal::StaticClass());

		if (bIsBase || bIsNormalEnemy)
		{
			FVector KnockbackDir = GetActorForwardVector();
			FVector LaunchForce = (KnockbackDir * 500.f) + FVector(0, 0, 1000.f); // 앞으로 살짝 + 위로 띄움
			VictimChar->LaunchCharacter(LaunchForce, true, true);
		}
	}
}

// [정지] 돌진 끝날 때 호출
void ATitanCharacter::StopCharge()
{
	bIsCharging = false;
	GetCharacterMovement()->StopMovementImmediately();

	// 물리 복구 (원래대로)
	GetCharacterMovement()->GroundFriction = DefaultGroundFriction;
	GetCharacterMovement()->GravityScale = DefaultGravityScale;

	// 충돌 설정 복구 (다시 막히게)
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);

	// 무시했던 애들 다시 인식하게 복구
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

	// 잡고 있는 대상을 네트워크 동기화 목록에 추가
	DOREPLIFETIME(ATitanCharacter, GrabbedActor);
}



// [벽 충돌] 벽에 박으면 멈춤
void ATitanCharacter::OnChargeHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority()) return;
	if (!bIsCharging) return;

	// 고정된 물체(벽)에 박으면 즉시 정지
	if (OtherActor->IsRootComponentStatic())
	{
		StopCharge();
	}
}