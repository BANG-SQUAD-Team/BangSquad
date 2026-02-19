#include "Project_Bang_Squad/Character/StageBoss/AnimNotify_SpiderMeleeHit.h"
#include "Project_Bang_Squad/Character/StageBoss/Stage2Boss.h" // Stage2Boss 헤더 포함

void UAnimNotify_SpiderMeleeHit::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	// 유효성 검사 (안전성 확보)
	if (!MeshComp || !MeshComp->GetOwner()) return;

	// [권한 분리] 데미지 판정은 무조건 서버에서만 실행되어야 하므로 Authority 체크
	if (MeshComp->GetOwner()->HasAuthority())
	{
		// 몽타주를 재생 중인 액터가 Stage2Boss인지 확인 후 평타 판정 함수 호출
		if (AStage2Boss* Boss = Cast<AStage2Boss>(MeshComp->GetOwner()))
		{
			Boss->PerformMeleeHitCheck();
		}
	}
}