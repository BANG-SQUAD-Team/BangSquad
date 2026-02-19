#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_SpiderFollowUp.generated.h"

/**
 * Smash 이후 추가타 몽타주에서 "지금 날려버려!" 시점에 호출
 */
UCLASS()
class PROJECT_BANG_SQUAD_API UAnimNotify_SpiderFollowUp : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
};