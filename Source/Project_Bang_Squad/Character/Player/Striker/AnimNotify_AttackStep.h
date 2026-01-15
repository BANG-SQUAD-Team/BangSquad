#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_AttackStep.generated.h"

/**
 * 공격 애니메이션 중 앞으로 이동하는 타이밍에 호출되는 노티파이
 */
UCLASS()
class PROJECT_BANG_SQUAD_API UAnimNotify_AttackStep : public UAnimNotify
{
	GENERATED_BODY()

public:
	// 애니메이션 시스템이 이 노티파이를 만났을 때 호출하는 함수 (Override)
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	// (옵션) 에디터에서 이동 힘을 덮어씌우고 싶다면 이 변수를 활용 가능
	// UPROPERTY(EditAnywhere, Category="Combat")
	// float OverrideForce = -1.0f; 
};