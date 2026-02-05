// Source/Project_Bang_Squad/AnimNotify/AnimNotify_Knockback.h

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_Knockback.generated.h"

/**
 * [넉백 전용 노티파이]
 * - 몽타주에 이 노티파이를 추가하면, 범위 내 적을 감지해 데미지를 주고 뒤로 밀어버립니다.
 */
UCLASS()
class PROJECT_BANG_SQUAD_API UAnimNotify_Knockback : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_Knockback();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	// 공격 범위 (반지름) - 기본값 150
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback Setting")
	float AttackRadius = 150.0f;

	// 공격 데미지 - 기본값 10
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback Setting")
	float DamageAmount = 10.0f;

	// 밀어내는 힘 (넉백 파워) - 기본값 800
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback Setting")
	float KnockbackStrength = 400.0f;

	// 위로 띄우는 힘 (마찰 무시용) - 기본값 250 (0이면 바닥에 끌려서 잘 안 밀림)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback Setting")
	float KnockbackUpForce = 150.0f;
};