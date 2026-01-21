#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h" // [핵심] 이걸 상속받습니다.
#include "AnimNotify_BossSlash.generated.h"

/**
 * [역할] 몽타주에서 이 노티파이가 찍힌 시점에
 * Stage1Boss의 AnimNotify_SpawnSlash 함수를 호출합니다.
 */
UCLASS()
class PROJECT_BANG_SQUAD_API UAnimNotify_BossSlash : public UAnimNotify
{
    GENERATED_BODY()

public:
    UAnimNotify_BossSlash();

    // 몽타주가 이 노티파이 지점을 지나갈 때 엔진이 자동으로 호출해주는 함수
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};