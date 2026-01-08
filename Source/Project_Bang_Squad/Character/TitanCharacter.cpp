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
			// ·Î±×·Î ÀâÀ¸·Á´Â ´ë»óÀÇ ÀÌ¸§À» ¸íÈ®È÷ È®ÀÎ
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

void ATitanCharacter::TryGrab()
{
	if (!HoveredActor)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("No target to grab!"));
		return;
	}

	GrabbedActor = HoveredActor;
	HoveredActor = nullptr;
	bIsGrabbing = true;

	SetHighlight(GrabbedActor, true);
	ProcessSkill(TEXT("JobAbility"));

	if (ACharacter* Victim = Cast<ACharacter>(GrabbedActor))
	{
		// [¼öÁ¤] Ä¸½¶ ÄÄÆ÷³ÍÆ®¸¦ ÅëÇØ ÇÔ¼ö È£Ãâ
		Victim->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// ¹°¸® ½Ã¹Ä·¹ÀÌ¼Ç ¼³Á¤
		Victim->GetMesh()->SetSimulatePhysics(true);
		Victim->GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));

		// [ÇÙ½É] ¼­·Î ¹Ð¾î³»Áö ¾Êµµ·Ï ¹«½Ã ¼³Á¤ (ÄÄÆ÷³ÍÆ® ´ÜÀ§)
		Victim->GetCapsuleComponent()->IgnoreActorWhenMoving(this, true);
		Victim->GetMesh()->IgnoreActorWhenMoving(this, true);
		this->GetCapsuleComponent()->IgnoreActorWhenMoving(Victim, true);

		// [Ãß°¡] ¾×ÅÍ ·¹º§¿¡¼­µµ ¼­·Î ¹«½ÃÇÏµµ·Ï ¼³Á¤ (¾ÈÀüÀåÄ¡)
		this->MoveIgnoreActorAdd(Victim);
		Victim->MoveIgnoreActorAdd(this);

		Victim->GetMesh()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}

	GrabbedActor->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Hand_R_Socket"));
	GetWorldTimerManager().SetTimer(GrabTimerHandle, this, &ATitanCharacter::ThrowTarget, GrabMaxDuration, false);
}

void ATitanCharacter::ThrowTarget()
{
	if (!bIsGrabbing || !GrabbedActor) return;

	GetWorldTimerManager().ClearTimer(GrabTimerHandle);
	SetHighlight(GrabbedActor, false);
	GrabbedActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	FVector ThrowDir = GetControlRotation().Vector();
	ThrowDir = (ThrowDir + FVector(0.f, 0.f, 0.2f)).GetSafeNormal();
	if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(GrabbedActor->GetRootComponent()))
	{
		RootComp->AddImpulse(ThrowDir * ThrowForce, NAME_None, true);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Orange, TEXT("Target Thrown!!"));
<<<<<<< Updated upstream
		RootComp->AddImpulse(ThrowDir * ThrowForce, NAME_None, true);
		float StrongerForce = 5000.f; // ï¿½ï¿½ï¿½ï¿½ 3500ï¿½ï¿½ï¿½ï¿½ ï¿½ï¿½ï¿½ï¿½ ï¿½ï¿½ï¿½ï¿½
		RootComp->AddImpulse(ThrowDir * StrongerForce, NAME_None, true); // ï¿½ï¿½ ï¿½ï¿½Â° ï¿½ï¿½ï¿½Ú°ï¿½ bVelChange

		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Orange, TEXT("Target Thrown Strongly!!"));
>>>>>>> Stashed changes
	}

	if (ACharacter* Victim = Cast<ACharacter>(GrabbedActor))
	{
		FTimerHandle RecoveryHandle;
		FTimerDelegate Delegate;
		Delegate.BindUFunction(this, TEXT("RecoverCharacter"), Victim);
		GetWorldTimerManager().SetTimer(RecoveryHandle, Delegate, 0.5f, false);
	}

	bIsGrabbing = false;
	GrabbedActor = nullptr;
	bIsCooldown = true;
	GetWorldTimerManager().SetTimer(CooldownTimerHandle, this, &ATitanCharacter::ResetCooldown, ThrowCooldownTime, false);
}

void ATitanCharacter::RecoverCharacter(ACharacter* Victim)
{
	if (!Victim || !Victim->IsValidLowLevel()) return;
	USkeletalMeshComponent* VictimMesh = Victim->GetMesh();
	UCapsuleComponent* Capsule = Victim->GetCapsuleComponent();
	FVector LandedLocation = VictimMesh->GetComponentLocation();
	float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	FVector NewLocation = LandedLocation + FVector(0.0f, 0.0f, CapsuleHalfHeight);
	Victim->SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);
	VictimMesh->AttachToComponent(Capsule, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	VictimMesh->SetSimulatePhysics(false);
	VictimMesh->SetCollisionProfileName(TEXT("CharacterMesh"));
	VictimMesh->SetRelativeLocation(FVector(0, 0, -90.0f));
	VictimMesh->SetRelativeRotation(FRotator(0, -90.0f, 0));
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	if (Victim->GetCharacterMovement()) { Victim->GetCharacterMovement()->SetMovementMode(MOVE_Walking); }
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