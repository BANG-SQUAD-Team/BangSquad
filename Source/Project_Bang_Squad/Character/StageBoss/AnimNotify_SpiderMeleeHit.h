#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_SpiderMeleeHit.generated.h"

/**
 * 거미 보스(Stage 2)의 기본 공격(평타) 타격 프레임에 호출되는 C++ 노티파이
 */
UCLASS()
class PROJECT_BANG_SQUAD_API UAnimNotify_SpiderMeleeHit : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
};