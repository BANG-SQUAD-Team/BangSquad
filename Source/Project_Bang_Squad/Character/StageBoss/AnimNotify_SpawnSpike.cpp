#include "AnimNotify_SpawnSpike.h"
#include "Project_Bang_Squad/Character/StageBoss/Stage1Boss.h"

void UAnimNotify_SpawnSpike::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (!MeshComp || !MeshComp->GetOwner()) return;

    // 보스 클래스로 캐스팅하여 함수 호출
    if (AStage1Boss* Boss = Cast<AStage1Boss>(MeshComp->GetOwner()))
    {
        // [중요]: Notify는 클라/서버 모두에서 호출될 수 있으므로,
        // 실제 스폰 로직 안에서 HasAuthority() 체크를 하거나 여기서 걸러줘야 함.
        // 안전하게 보스에게 "실행해"라고 명령만 내림.
        Boss->ExecuteSpikeSpell();
    }
}