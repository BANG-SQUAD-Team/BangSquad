// Source/Project_Bang_Squad/Character/StageBoss/AnimNotify_BossWebShot.cpp

#include "Project_Bang_Squad/Character/StageBoss/AnimNotify_BossWebShot.h"
#include "Project_Bang_Squad/Character/StageBoss/Stage2Boss.h" // 보스 헤더 참조

void UAnimNotify_BossWebShot::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		// 보스를 찾아서 "쏴라!" 명령 (FireWebProjectile 함수 호출)
		if (AStage2Boss* Boss = Cast<AStage2Boss>(MeshComp->GetOwner()))
		{
			Boss->FireWebProjectile();
		}
	}
}