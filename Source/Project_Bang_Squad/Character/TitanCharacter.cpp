#include "Project_Bang_Squad/Character/TitanCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"

ATitanCharacter::ATitanCharacter()
{
	GetCharacterMovement()->MaxWalkSpeed = 450.f; // 타이탄은 조금 느리게 설정
}

void ATitanCharacter::JobAbility()
{
	if (!bIsGrabbing) {
		TryGrab();
	}
	else {
		ThrowTarget();
	}
}

void ATitanCharacter::TryGrab()
{
	// 전방 작은 범위에 잡기 판정 (SphereTrace)
	FVector Start = GetActorLocation() + GetActorForwardVector() * 50.f;
	FVector End = Start + GetActorForwardVector() * 100.f;
	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(this);
	FHitResult HitResult;

	bool bHit = UKismetSystemLibrary::SphereTraceSingle(GetWorld(), Start, End, 50.f,
		UEngineTypes::ConvertToTraceType(ECC_Pawn), false, IgnoreActors, EDrawDebugTrace::ForDuration, HitResult, true);

	if (bHit && HitResult.GetActor())
	{
		GrabbedActor = HitResult.GetActor();
		// TODO: 대상이 보스인지 체크 로직 추가 필요

		GrabbedActor->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Hand_R_Socket"));
		bIsGrabbing = true;

		// 5초 후 자동 던지기 타이머
		GetWorldTimerManager().SetTimer(GrabTimerHandle, this, &ATitanCharacter::ThrowTarget, 5.0f, false);

		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, TEXT("Target Grabbed!"));
	}
}

void ATitanCharacter::ThrowTarget()
{
	if (!bIsGrabbing || !GrabbedActor) return;

	GetWorldTimerManager().ClearTimer(GrabTimerHandle);

	GrabbedActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// 바라보는 방향으로 던지기 (Launch 처리)
	FVector ThrowDir = GetControlRotation().Vector();
	// 대상이 캐릭터라면 LaunchCharacter, 일반 액터라면 AddImpulse 사용
	// GrabbedActor->AddImpulse(ThrowDir * 2000.f); 

	bIsGrabbing = false;
	GrabbedActor = nullptr;
	GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Orange, TEXT("Target Thrown!"));
}

void ATitanCharacter::Skill1() { ProcessSkill(TEXT("Skill1")); /* 바위 소환 및 던지기 로직 */ }
void ATitanCharacter::Skill2() { ProcessSkill(TEXT("Skill2")); /* 저지불가 돌진 로직 */ }

void ATitanCharacter::ProcessSkill(FName SkillRowName)
{
	// MageCharacter와 유사하게 데이터 테이블에서 정보를 가져와 처리합니다.
}