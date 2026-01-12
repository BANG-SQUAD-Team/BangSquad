#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_TitanGrab.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API UAnimNotify_TitanGrab : public UAnimNotify
{
	GENERATED_BODY()

public:
	// 노티파이가 호출될 때 실행되는 함수 (오버라이드)
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
};