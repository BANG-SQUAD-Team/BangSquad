#include "TitanShopMannequin.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"

ATitanShopMannequin::ATitanShopMannequin()
{
    // ATitanCharacter의 생성자가 먼저 호출되므로, 
    // CharacterSpecificAccessoryOffset(-20) 설정 로직은 이미 처리되어 있습니다.

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