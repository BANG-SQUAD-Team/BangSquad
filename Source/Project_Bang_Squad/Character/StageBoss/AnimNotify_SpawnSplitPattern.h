#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_SpawnSplitPattern.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API UAnimNotify_SpawnSplitPattern : public UAnimNotify
{
	GENERATED_BODY()

public:
	// 몽타주에서 Notify 트랙을 지날 때 실행됨
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};