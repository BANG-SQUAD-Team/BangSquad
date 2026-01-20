#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "TitanNotify_RockSkill.generated.h"

// -------------------------------------------------------------------------
// 클래스 1: 돌 생성 (손에 붙이기)
// -------------------------------------------------------------------------
UCLASS()
class PROJECT_BANG_SQUAD_API UAnimNotify_SpawnRock : public UAnimNotify
{
	GENERATED_BODY()
public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};

// -------------------------------------------------------------------------
// 클래스 2: 돌 던지기 (발사)
// -------------------------------------------------------------------------
UCLASS()
class PROJECT_BANG_SQUAD_API UAnimNotify_ThrowRock : public UAnimNotify
{
	GENERATED_BODY()
public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};