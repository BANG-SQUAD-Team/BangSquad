#include "Project_Bang_Squad/Character/StageBoss/AnimNotify_SpawnBossLift.h"
#include "Project_Bang_Squad/Character/StageBoss/BossLiftPillar.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"

void UAnimNotify_SpawnBossLift::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner()) return;

	AActor* OwnerActor = MeshComp->GetOwner();

	if (OwnerActor->HasAuthority())
	{
		if (!LiftClass) return;

		FVector SpawnLoc = OwnerActor->GetActorLocation();

		if (ACharacter* BossChar = Cast<ACharacter>(OwnerActor))
		{
			float HalfHeight = BossChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			SpawnLoc.Z -= (HalfHeight + 10.f);
		}
		else
		{
			SpawnLoc.Z -= 100.f;
		}

		FActorSpawnParameters Params;
		Params.Owner = nullptr; // 충돌을 위해 nullptr 유지
		Params.Instigator = Cast<APawn>(OwnerActor);
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ABossLiftPillar* Pillar = OwnerActor->GetWorld()->SpawnActor<ABossLiftPillar>(LiftClass, SpawnLoc, FRotator::ZeroRotator, Params);

		if (IsValid(Pillar))
		{
			// [수정] 에디터에서 설정한 LifeTimeOffset 값도 함께 전달
			Pillar->InitializeLift(TargetHeight, RiseDuration, LifeTimeOffset);
		}
	}
}