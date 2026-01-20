#include "TitanNotify_RockSkill.h"
#include "Project_Bang_Squad/Character/TitanCharacter.h" // 타이탄 헤더

// =========================================================================
// 1. 돌 생성 구현
// =========================================================================
void UAnimNotify_SpawnRock::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		ATitanCharacter* Titan = Cast<ATitanCharacter>(MeshComp->GetOwner());
		if (Titan)
		{
			// TitanCharacter.cpp에 만든 'ExecuteSpawnRock' 호출
			Titan->ExecuteSpawnRock();
		}
	}
}

// =========================================================================
// 2. 돌 던지기 구현
// =========================================================================
void UAnimNotify_ThrowRock::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		ATitanCharacter* Titan = Cast<ATitanCharacter>(MeshComp->GetOwner());
		if (Titan)
		{
			// TitanCharacter.cpp에 만든 'ExecuteThrowRock' 호출
			Titan->ExecuteThrowRock();
		}
	}
}