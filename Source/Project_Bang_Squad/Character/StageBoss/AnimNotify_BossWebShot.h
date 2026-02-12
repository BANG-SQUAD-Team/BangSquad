// Source/Project_Bang_Squad/Character/StageBoss/AnimNotify_BossWebShot.h

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_BossWebShot.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API UAnimNotify_BossWebShot : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};