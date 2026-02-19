#include "Project_Bang_Squad/Character/StageBoss/AQTE_Trap.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Project_Bang_Squad/Character/StageBoss/StageBossPlayerController.h" // 경로 맞춰주세요

AQTE_Trap::AQTE_Trap()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    RootComponent = MeshComp;
    MeshComp->SetCollisionProfileName(TEXT("NoCollision"));
}

void AQTE_Trap::InitializeTrap(ACharacter* TargetPlayer, int32 RequiredMashCount)
{
    if (!HasAuthority() || !TargetPlayer) return;

    TrappedPlayer = TargetPlayer;
    TargetCount = RequiredMashCount;
    CurrentCount = 0;

    // 1. 플레이어에게 부착
    AttachToActor(TrappedPlayer, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    AddActorLocalOffset(FVector(0.f, 0.f, -50.f));

    // 2. 이동 능력 상실 (BaseCharacter 코드 수정 없이 기본 컴포넌트 제어)
    if (UCharacterMovementComponent* Movement = TrappedPlayer->GetCharacterMovement())
    {
        Movement->SetMovementMode(MOVE_None);
    }

    // 3. 컨트롤러를 찾아 QTE 모드 진입 지시
    TrappedPC = Cast<AStageBossPlayerController>(TrappedPlayer->GetController());
    if (TrappedPC)
    {
        TrappedPC->Server_SetQTETrap(this); // 컨트롤러에 트랩 등록 및 클라이언트 UI 갱신 지시
    }
}

void AQTE_Trap::AddQTEProgress()
{
    if (!HasAuthority()) return;

    CurrentCount++;

    // UI 업데이트를 위해 클라이언트로 RPC 전송
    if (TrappedPC)
    {
        TrappedPC->Client_UpdateIndividualQTEUI(CurrentCount, TargetCount);
    }

    if (CurrentCount >= TargetCount)
    {
        BreakTrap();
    }
}

void AQTE_Trap::BreakTrap()
{
    if (!HasAuthority()) return;

    // 1. 이동 능력 복구
    if (TrappedPlayer)
    {
        if (UCharacterMovementComponent* Movement = TrappedPlayer->GetCharacterMovement())
        {
            Movement->SetMovementMode(MOVE_Walking);
        }
    }

    // 2. 컨트롤러 QTE 모드 해제 지시
    if (TrappedPC)
    {
        TrappedPC->Server_ClearQTETrap();
    }

    Destroy();
}