#include "Project_Bang_Squad/Character/StageBoss/AnimNotify_SpawnSplitPattern.h"
#include "Project_Bang_Squad/Character/StageBoss/Stage2Boss.h"

void UAnimNotify_SpawnSplitPattern::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		if (AStage2Boss* Boss = Cast<AStage2Boss>(MeshComp->GetOwner()))
		{
			// 서버에서만 스폰되도록 내부에서 방어되어 있음
			Boss->SpawnSplitPattern();
		}
	}
}