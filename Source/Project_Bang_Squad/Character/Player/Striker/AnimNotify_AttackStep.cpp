#include "Project_Bang_Squad/Character/Player/Striker/AnimNotify_AttackStep.h"
#include "Project_Bang_Squad/Character/StrikerCharacter.h" // Striker 헤더 포함 필수

void UAnimNotify_AttackStep::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner()) return;

	// 오너가 StrikerCharacter인지 확인하고 캐스팅
	AStrikerCharacter* Striker = Cast<AStrikerCharacter>(MeshComp->GetOwner());
	if (Striker)
	{
		// Striker 클래스에 새로 만들 함수를 호출
		Striker->ApplyAttackForwardForce();
	}
}