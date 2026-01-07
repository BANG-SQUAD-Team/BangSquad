#include "Project_Bang_Squad/Character/TitanCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"

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

	// 잡고 있지 않을 때만 하이라이트 프리뷰 작동
	if (!bIsGrabbing)
	{
		UpdateHoverHighlight();
	}
}

void ATitanCharacter::JobAbility()
{
	if (bIsCooldown) return; // 쿨타임 중이면 무시

	if (!bIsGrabbing) {
		TryGrab();
	}
	else {
		ThrowTarget();
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
		// 보스나 중간보스 태그가 없을 때만 타겟으로 인정
		if (!HitActor->ActorHasTag("Boss") && !HitActor->ActorHasTag("MidBoss"))
		{
			NewTarget = HitActor;
		}
	}

	// 쳐다보는 대상이 바뀌었을 때만 하이라이트 갱신
	if (HoveredActor != NewTarget)
	{
		if (HoveredActor) SetHighlight(HoveredActor, false);
		if (NewTarget) SetHighlight(NewTarget, true);
		HoveredActor = NewTarget;
	}
}

void ATitanCharacter::TryGrab()
{
	// 현재 하이라이트 된 대상이 있으면 바로 잡기
	if (!HoveredActor) return;

	GrabbedActor = HoveredActor;
	HoveredActor = nullptr; // 잡는 순간 프리뷰 해제

	bIsGrabbing = true;

	// 하이라이트는 유지 (잡힌 상태 표시)
	SetHighlight(GrabbedActor, true);

	if (ACharacter* Victim = Cast<ACharacter>(GrabbedActor))
	{
		// 랙돌 설정
		Victim->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Victim->GetMesh()->SetSimulatePhysics(true);
		Victim->GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	}

	// 타이탄의 손 소켓에 부착
	GrabbedActor->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Hand_R_Socket"));

	// 5초 자동 던지기 타이머
	GetWorldTimerManager().SetTimer(GrabTimerHandle, this, &ATitanCharacter::ThrowTarget, GrabMaxDuration, false);
}

void ATitanCharacter::ThrowTarget()
{
	if (!bIsGrabbing || !GrabbedActor) return;

	GetWorldTimerManager().ClearTimer(GrabTimerHandle);
	SetHighlight(GrabbedActor, false);

	GrabbedActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	FVector ThrowDir = GetControlRotation().Vector();
	if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(GrabbedActor->GetRootComponent()))
	{
		// 강력하게 던지기
		RootComp->AddImpulse(ThrowDir * ThrowForce, NAME_None, true);
	}

	if (ACharacter* Victim = Cast<ACharacter>(GrabbedActor))
	{
		FTimerHandle RecoveryHandle;
		FTimerDelegate Delegate;
		Delegate.BindUFunction(this, TEXT("RecoverCharacter"), Victim);

		// 0.5초 뒤 도착지에서 일어남
		GetWorldTimerManager().SetTimer(RecoveryHandle, Delegate, 0.5f, false);
	}

	bIsGrabbing = false;
	GrabbedActor = nullptr;

	// 던진 후 쿨타임 시작
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
	if (Victim->GetCharacterMovement())
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
}

void ATitanCharacter::Skill1() { /* 바위 소환 로직 */ }
void ATitanCharacter::Skill2() { /* 돌진 로직 */ }