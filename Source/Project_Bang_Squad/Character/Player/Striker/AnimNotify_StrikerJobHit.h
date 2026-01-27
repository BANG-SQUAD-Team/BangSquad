#pragma once

#include "CoreMinimal.h" 
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_StrikerJobHit.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API UAnimNotify_StrikerJobHit : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
};