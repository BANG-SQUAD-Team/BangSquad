
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
	MeshComp->SetCustomDepthStencilValue(251); // 노란색 등 설정값
	
	NetUpdateFrequency = 100.0f;
	MinNetUpdateFrequency = 60.0f;
	NetPriority = 3.0f;
}

// Called when the game starts or when spawned
void AMagicBoat::BeginPlay()
{
	Super::BeginPlay();
	
}


void AMagicBoat::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// 서버 권한이 있고 입력이 있을 때만 힘을 줌
	if (HasAuthority() && MeshComp)
	{
		if (HasAuthority() && MeshComp && !CurrentMoveDirection.IsNearlyZero() && bIsOccupied)
		{
			MeshComp->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
			// 내적 연산 
			// 결과 양수면 전진, 음수면 후진
			float ForwardInput = FVector::DotProduct(CurrentMoveDirection, GetActorForwardVector());
		
			// 보트가 바라보는 방향으로 힘을 줌
			FVector MoveForce = GetActorForwardVector() * ForwardInput * MoveSpeed * MeshComp->GetMass();
			MeshComp->AddForce(MoveForce);
			
		}
		// 입력이 없을때 (정지 처리)
		else
		{
			// 손 떼면 마찰력 높여서 브레이크
			MeshComp->SetLinearDamping(5.0f);
			
			// 속도가 거의 0이면 확실하게 멈춤
			if (MeshComp->GetPhysicsLinearVelocity().SizeSquared() < 100.0f)
			{
				MeshComp->SetPhysicsLinearVelocity(FVector::ZeroVector);
			}
		}
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

void AMagicBoat::SetRideState(bool bRiding)
{
	bIsOccupied = bRiding;
	
	// 내렸을 때 이동 명령도 초기화
	if (!bIsOccupied)
	{
		CurrentMoveDirection = FVector::ZeroVector;
	}
}

