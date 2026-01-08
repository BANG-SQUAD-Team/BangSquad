#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "EnemyAnimInstance.generated.h"

/**
 * Enemy용 공통 AnimInstance
 * - Speed를 C++에서 계산해서 AnimBP가 그대로 사용하게 함
 */
UCLASS()
class PROJECT_BANG_SQUAD_API UEnemyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** BlendSpace 1D 입력으로 쓸 이동 속도 */
	UPROPERTY(BlueprintReadOnly, Category = "Anim|Locomotion")
	float Speed = 0.f;

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	/** 소유 Pawn 캐시(매 프레임 TryGetPawnOwner 호출 줄이기) */
	TWeakObjectPtr<APawn> CachedPawn;
};
