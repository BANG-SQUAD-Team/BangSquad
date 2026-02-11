// ShopMannequin.cpp
#include "ShopMannequin.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"

AShopMannequin::AShopMannequin()
{
	// 1. 틱(Tick) 끄기 : 매 프레임 계산하는 거 중지 (제일 중요 ⭐)
	PrimaryActorTick.bCanEverTick = false;

	// 2. 이동 컴포넌트 무력화
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->GravityScale = 0.0f; // 중력 끄기
		GetCharacterMovement()->MaxWalkSpeed = 0.0f; // 이동 속도 0
		GetCharacterMovement()->bRunPhysicsWithNoController = false; // 물리 연산 끄기
	}

	// 3. 충돌(Collision) 끄기 : 물리 계산 비용 삭제
	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (GetMesh())
	{
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// "안 보여도 애니메이션은 계속 돌려라" (미리보기용 필수 설정)
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}

	// 4. AI 컨트롤러 빙의 방지
	AutoPossessAI = EAutoPossessAI::Disabled;
}

void AShopMannequin::BeginPlay()
{
	// 1. [필수] 부모님(BaseCharacter -> Actor)의 BeginPlay를 실행해서 컴포넌트를 깨움
	Super::BeginPlay();

	// 2. [확인사살] 강제로 애니메이션 재생 명령 (혹시 몰라서 넣는 보험)
	if (GetMesh())
	{
		// 모드 확실히 박기
		GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);

		// *중요* : 여기서 님이 쓰고 싶은 Idle 애니메이션을 코드로 박아버리면 100% 됨
		// (지금은 블루프린트 설정 믿고 일단 비워둠)
		// GetMesh()->PlayAnimation(내_Idle_애니메이션, true); 
	}
}