#include "AnimNotify_BossSlash.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h" // 부모 클래스 헤더 참조

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

    // Stage1Boss 대신 공통 부모인 EnemyCharacterBase로 캐스팅
    if (AEnemyCharacterBase* Boss = Cast<AEnemyCharacterBase>(MeshComp->GetOwner()))
    {
        Boss->AnimNotify_SpawnSlash(); // 부모에 정의된 함수 호출
    }
}