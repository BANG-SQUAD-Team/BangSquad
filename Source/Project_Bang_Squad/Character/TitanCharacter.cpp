#include "Project_Bang_Squad/Character/TitanCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h" // SuggestProjectileVelocity용
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

void ATitanCharacter::ProcessSkill(FName SkillRowName)
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
			FString::Printf(TEXT("Titan %s Activated! (Damage: %.1f)"), *Data->SkillName.ToString(), Data->Damage));
		if (Data->SkillMontage) { PlayAnimMontage(Data->SkillMontage); }
	}
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

// ==========================================================
// [잡기] 물리 끄고 소켓에 부착 (랙돌 X)
// ==========================================================
void ATitanCharacter::TryGrab()
{
	if (!HoveredActor) return;

	GrabbedActor = HoveredActor;
	HoveredActor = nullptr;
	bIsGrabbing = true;

	SetHighlight(GrabbedActor, true);
	ProcessSkill(TEXT("JobAbility"));

	if (ABaseCharacter* Victim = Cast<ABaseCharacter>(GrabbedActor))
	{
		// 1. 캡슐 충돌 끄기 (잡혀 있는 동안)
		Victim->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// 2. 물리 끄기 (확실하게 고정)
		Victim->GetMesh()->SetSimulatePhysics(false);

		// 3. 잡힘 상태 설정
		Victim->SetIsGrabbed(true);

		// 4. 상호 무시
		this->MoveIgnoreActorAdd(Victim);
		Victim->MoveIgnoreActorAdd(this);
	}

	// 5. 소켓 부착
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
		Victim->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Victim->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

		Victim->GetCharacterMovement()->SetMovementMode(MOVE_Falling);

		// 1. 비거리를 유지하면서 시각적 해상도를 높임
		FVector StartPos = Victim->GetActorLocation();
		FVector ForwardVector = GetControlRotation().Vector();
		FVector TargetPos = StartPos + (ForwardVector * 2000.0f);

		FVector LaunchVelocity;
		// 2. [핵심] ArcParam을 0.85로 설정 (거의 산 모양으로 높게 던짐)
		// 1.0에 가까울수록 정점이 높아집니다.
		bool bHaveSolution = UGameplayStatics::SuggestProjectileVelocity_CustomArc(
			this, LaunchVelocity, StartPos, TargetPos, 0.0f, 0.85f);

		if (!bHaveSolution)
		{
			// 계산 실패 시 보정값: 위로 더 강하게 솟구치게 설정
			LaunchVelocity = ForwardVector * 1500.0f + FVector(0, 0, 1200.0f);
		}

		// 3. 발사!
		Victim->LaunchCharacter(LaunchVelocity, true, true);

		// 4. [중요] 중력을 4.5로 높여서 정점에 도달한 뒤 순식간에 바닥으로 꽂히게 함
		Victim->SetThrownByTitan(true, this);
		Victim->GetCharacterMovement()->GravityScale = 4.5f;
		Victim->GetCharacterMovement()->AirControl = 0.0f;

		Victim->SetIsGrabbed(false);
		this->MoveIgnoreActorRemove(Victim);
		Victim->MoveIgnoreActorRemove(this);
	}

	bIsGrabbing = false;
	GrabbedActor = nullptr;
	bIsCooldown = true;
	GetWorldTimerManager().SetTimer(CooldownTimerHandle, this, &ATitanCharacter::ResetCooldown, ThrowCooldownTime, false);
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