#include "Project_Bang_Squad/Character/StageBoss/AnimNotify_SpiderFollowUp.h"
#include "Project_Bang_Squad/Character/StageBoss/Stage2Boss.h"

void UAnimNotify_SpiderFollowUp::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	if (!MeshComp || !MeshComp->GetOwner()) return;

	// [권한 분리] 물리력을 가하는 로직이므로 서버에서만 실행
	if (MeshComp->GetOwner()->HasAuthority())
	{
		if (AStage2Boss* Boss = Cast<AStage2Boss>(MeshComp->GetOwner()))
		{
			Boss->ExecuteFollowUpKnockback();
		}
	}
}