#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_SpawnBossLift.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API UAnimNotify_SpawnBossLift : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	// [설정] 기둥이 얼마나 높이 올라갈지
	UPROPERTY(EditAnywhere, Category = "BossLift")
	float TargetHeight = 400.f;

	// [설정] 기둥이 올라가는 데 걸리는 시간
	UPROPERTY(EditAnywhere, Category = "BossLift")
	float RiseDuration = 0.8f;

	// [신규 설정] 다 올라간 뒤, 몇 초 동안 유지될 것인가? (기본 10초)
	UPROPERTY(EditAnywhere, Category = "BossLift")
	float LifeTimeOffset = 10.0f;

	// [핵심] 여기에 BP_BossLiftPillar 할당
	UPROPERTY(EditAnywhere, Category = "BossLift")
	TSubclassOf<class ABossLiftPillar> LiftClass;
};