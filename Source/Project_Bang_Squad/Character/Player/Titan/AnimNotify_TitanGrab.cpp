#include "AnimNotify_TitanGrab.h"
#include "Project_Bang_Squad/Character/TitanCharacter.h" // 타이탄 헤더 경로 확인하세요!

void UAnimNotify_TitanGrab::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::Notify(MeshComp, Animation);

	if (MeshComp && MeshComp->GetOwner())
	{
		// 1. 노티파이를 호출한 주인이 타이탄인지 확인
		ATitanCharacter* Titan = Cast<ATitanCharacter>(MeshComp->GetOwner());
		if (Titan)
		{
			// 2. 타이탄의 잡기 함수를 강제로 실행!
			Titan->ExecuteGrab();

			// 디버그 로그
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, TEXT("Custom Notify: ExecuteGrab Called!"));
		}
	}
}