
#include "Project_Bang_Squad/Character/Player/Mage/MagicBoat.h"

AMagicBoat::AMagicBoat()
{
 	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;
	
	// 물리 설정
	MeshComp->SetSimulatePhysics(true);
	MeshComp->SetLinearDamping(5.0f); // 미끄러짐 방지
	MeshComp->SetAngularDamping(2.0f); // 회전 저항
	
	// 아웃라인 준비 (CustomDepth)
	MeshComp->SetRenderCustomDepth(false);
	MeshComp->SetCustomDepthStencilValue(250); // 노란색 등 설정값
}

// Called when the game starts or when spawned
void AMagicBoat::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AMagicBoat::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// 서버 권한이 있고 입력이 있을 때만 힘을 줌
	if (HasAuthority() && MeshComp && !CurrentMoveDirection.IsNearlyZero())
	{
		// 1. 이동 처리
		// GetMass()를 곱해주면 보트 무게를 100kg로 하든 10,000kg로 하든 똑같은 속도로 움직임!
		FVector Force = CurrentMoveDirection * MoveSpeed * MeshComp->GetMass();
		MeshComp->AddForce(Force);
		
		// 2. 회전 처리 (보트가 가능 방향을 바라보게)
		FRotator TargetRot = CurrentMoveDirection.Rotation();
		// 기울어짐 방지
		FRotator NewRot = FRotator(0.0f, TargetRot.Yaw, 0.0f);
		
		// 부드럽게 회전
		
		FRotator SmoothRot = FMath::RInterpTo(GetActorRotation(), NewRot, DeltaTime, 3.0f);
		SetActorRotation(SmoothRot);
	}

}

void AMagicBoat::SetMageHighlight(bool bActive)
{
	if (MeshComp)
	{
		MeshComp->SetRenderCustomDepth(bActive);
	}
}

void AMagicBoat::ProcessMageInput(FVector Direction)
{
	// 물리력은 서버에서만 가해야 함 (위치 동기화를 위해)
	if (HasAuthority())
	{
		// 입력받은 벡터를 정규화해서 저장 (방향만 가짐)
		// 만약 Direction이 (0,0,0)이면 멈추게 됨
		CurrentMoveDirection = Direction.GetSafeNormal();
		CurrentMoveDirection.Z = 0.0f; // 위아래 이동 차단
	}
}

