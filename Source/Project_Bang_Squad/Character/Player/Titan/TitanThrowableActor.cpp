#include "Project_Bang_Squad/Character/Player/Titan/TitanThrowableActor.h"

ATitanThrowableActor::ATitanThrowableActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// 1. 멀티플레이 이동 동기화 (서버에서 던지면 클라도 날아가게)
	bReplicates = true;
	SetReplicateMovement(true);

	// 2. 메쉬 컴포넌트 설정
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;

	// [중요] 물체가 움직이려면 반드시 Movable이어야 함 (Static이면 공중 부양함)
	MeshComp->SetMobility(EComponentMobility::Movable);

	// 3. 물리 및 충돌 기본 설정
	// (던지기 전에는 꺼져있을 수도 있지만, 기본적으로 켜두는 세팅)
	MeshComp->SetSimulatePhysics(true);
	MeshComp->SetEnableGravity(true);

	// PhysicsActor: 물리 연산용 프리셋 (프로젝트 설정에 따라 다를 수 있으니 확인 필요)
	// 만약 커스텀 프리셋이 없다면 'PhysicsActor' 대신 'BlockAllDynamic' 등을 고려
	MeshComp->SetCollisionProfileName(TEXT("PhysicsActor"));
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	// 무게 설정 (너무 가벼우면 종이처럼 날아가고, 너무 무거우면 안 날아감)
	MeshComp->SetMassOverrideInKg(NAME_None, 50.0f);
}

void ATitanThrowableActor::BeginPlay()
{
	Super::BeginPlay();

	// [핵심] 게임 시작 시 강제로 물리와 중력을 켜버립니다.
	// 블루프린트 설정이 꼬여있어도 이 코드가 이깁니다.
	if (MeshComp)
	{
		MeshComp->SetMobility(EComponentMobility::Movable);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MeshComp->SetSimulatePhysics(true); // 물리 켜기
		MeshComp->SetEnableGravity(true);   // 중력 켜기

		// (선택) 너무 가벼우면 안 떨어지는 느낌이 날 수 있으니 무게 설정
		MeshComp->SetMassOverrideInKg(NAME_None, 50.0f);
	}
}
