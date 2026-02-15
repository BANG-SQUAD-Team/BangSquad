#include "MageShopMannequin.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

AMageShopMannequin::AMageShopMannequin()
{
    // MageCharacter의 지팡이 부착 로직은 생성자 체인에 의해 자동으로 실행됨

    // 1. 마네킹 전용 최적화: 기능 끄기
    PrimaryActorTick.bCanEverTick = false;

    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        MoveComp->GravityScale = 0.0f;
        MoveComp->MaxWalkSpeed = 0.0f;
        MoveComp->bRunPhysicsWithNoController = false;
        MoveComp->SetComponentTickEnabled(false); // 이동 틱도 중지
    }

    // 2. 충돌 제거
    if (GetCapsuleComponent())
    {
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    if (GetMesh())
    {
        GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        // 마네킹이므로 애니메이션은 항상 갱신되도록 설정
        GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    }

    // 3. AI 관련 기능 배제
    AutoPossessAI = EAutoPossessAI::Disabled;
}