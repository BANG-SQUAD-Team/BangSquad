#include "AnimNotify_BossSlash.h"
#include "Project_Bang_Squad/Character/StageBoss/Stage1Boss.h" // 보스 헤더 참조

UAnimNotify_BossSlash::UAnimNotify_BossSlash()
{
    // 에디터 몽타주 트랙에서 보일 색상 (붉은색 추천)
#if WITH_EDITORONLY_DATA
    NotifyColor = FColor(255, 100, 100, 255);
#endif
}

void UAnimNotify_BossSlash::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (!MeshComp || !MeshComp->GetOwner()) return;

    // 1. 이 몽타주를 재생 중인 액터가 'Stage1Boss'인지 확인
    if (AStage1Boss* Boss = Cast<AStage1Boss>(MeshComp->GetOwner()))
    {
        // 2. 보스의 발사 함수 호출 (보스 헤더에서 public이어야 가능)
        Boss->AnimNotify_SpawnSlash();
    }
}