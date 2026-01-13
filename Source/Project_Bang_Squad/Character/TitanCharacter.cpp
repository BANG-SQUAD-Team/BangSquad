#include "Project_Bang_Squad/Character/TitanCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"
#include "Engine/DataTable.h"

ATitanCharacter::ATitanCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	GetCharacterMovement()->MaxWalkSpeed = 450.f;
}

void ATitanCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void ATitanCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bIsGrabbing) { UpdateHoverHighlight(); }
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

	if (ACharacter* Victim = Cast<ACharacter>(GrabbedActor))
	{
		// 물리를 끕니다 (튕겨나감 방지)
		Victim->GetMesh()->SetSimulatePhysics(false);
		Victim->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 손에 부착! (이게 8프레임에 실행됨)
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

	// 손에서 떼기
	GrabbedActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	if (ACharacter* Victim = Cast<ACharacter>(GrabbedActor))
	{
		// 1. 잡혔을 때 껐던 캡슐 콜리전 복구 (이거 안 켜면 땅 뚫고 떨어짐)
		Victim->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		// 2. 물리 시뮬레이션 끄기 (혹시 켜져있을 경우 대비 안전장치)
		Victim->GetMesh()->SetSimulatePhysics(false);

		// 3. 던지는 방향 및 힘 계산
		// LaunchCharacter는 질량 무시하고 속도를 바로 꽂으므로 1500~4000 정도면 충분함
		FVector ThrowDir = GetControlRotation().Vector();
		ThrowDir = (ThrowDir + FVector(0.f, 0.f, 0.25f)).GetSafeNormal(); // 살짝 위로

		// ThrowForce (헤더에 있는 3500.f 사용)
		FVector LaunchVelocity = ThrowDir * ThrowForce;

		// 4. Launch Character 발동! (XY, Z 모두 override해서 강제로 날림)
		Victim->LaunchCharacter(LaunchVelocity, true, true);
	}

	// 5. 기상/상태 복구 타이머 (Launch는 착지하면 알아서 걷기로 바뀌지만, 안전하게 상태 확인)
	if (ACharacter* Victim = Cast<ACharacter>(GrabbedActor))
	{
		FTimerHandle RecoveryHandle;
		FTimerDelegate Delegate;
		Delegate.BindUFunction(this, TEXT("RecoverCharacter"), Victim);
		// 날아가는 시간 대충 고려해서 1.5초 뒤 체크
		GetWorldTimerManager().SetTimer(RecoveryHandle, Delegate, 1.5f, false);
	}

	bIsGrabbing = false;
	GrabbedActor = nullptr;
	bIsCooldown = true;
	GetWorldTimerManager().SetTimer(CooldownTimerHandle, this, &ATitanCharacter::ResetCooldown, ThrowCooldownTime, false);
}

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

void ATitanCharacter::Attack()
{
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
	// Launch Character는 랙돌과 달리 메시가 분리되지 않으므로 복잡한 복구 로직이 필요 없음.
	// 혹시 모를 꼬임 방지용 안전장치만 수행.

	if (!Victim || !Victim->IsValidLowLevel()) return;

	// 캡슐 콜리전 확실하게 켜기
	if (Victim->GetCapsuleComponent()->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics)
	{
		Victim->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	// 움직임 모드 확인 (날아가다가 끼었을 때 강제로 걷기로 전환)
	if (Victim->GetCharacterMovement() && Victim->GetCharacterMovement()->MovementMode == MOVE_None)
	{
		Victim->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
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

void ATitanCharacter::Skill1() { ProcessSkill(TEXT("Skill1")); }
void ATitanCharacter::Skill2() { ProcessSkill(TEXT("Skill2")); }