
#include "Project_Bang_Squad/Character/Player/Mage/MagicBoat.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Net/UnrealNetwork.h"

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
	
	SetNetUpdateFrequency(100.0f);
	SetMinNetUpdateFrequency(60.0f);
	NetPriority = 3.0f;
	
	// 탑승자 감지용 박스 컴포넌트 생성
	PassengerCheckVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("PassengerCheckVolume"));
	PassengerCheckVolume->SetupAttachment(RootComponent);
	// 박스 크기 (에디터에서 조절 가능)
	PassengerCheckVolume->SetBoxExtent(FVector(200.f, 100.f, 100.f));
	PassengerCheckVolume->SetRelativeLocation(FVector(0.f,0.f,50.f));
	PassengerCheckVolume->SetCollisionProfileName(TEXT("Trigger"));
}

// Called when the game starts or when spawned
void AMagicBoat::BeginPlay()
{
	Super::BeginPlay();
	
	// 1. 게임 시작 시점 (에디터 배치 위치)을 저장
	InitialLocation = GetActorLocation();
	InitialRotation = GetActorRotation();
	
}

void AMagicBoat::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	// bIsOccupied변수 동기화 등록
	DOREPLIFETIME(AMagicBoat, bIsOccupied);
}

void AMagicBoat::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// 서버 권한이 있고 입력이 있을 때만 힘을 줌
	if (HasAuthority() && MeshComp)
	{
		if (!CurrentMoveDirection.IsNearlyZero() && bIsOccupied)
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

void AMagicBoat::ResetToStart(FVector SafeLandingLocation)
{
	// 서버에서만 실행 (위치 동기화를 위해)
	if (!HasAuthority()) return;
    
	// =========================================================
	// 1. [탑승자 처리]
	// =========================================================
	TArray<AActor*> OverlappingActors;
	if (PassengerCheckVolume)
	{
		PassengerCheckVolume->GetOverlappingActors(OverlappingActors, ABaseCharacter::StaticClass());
	}
    
	//  반복문 시작
	for (AActor* Actor : OverlappingActors)
	{
		ABaseCharacter* BaseChar = Cast<ABaseCharacter>(Actor);
		if (BaseChar)
		{
			if (BaseChar->GetCharacterMovement())
			{
				BaseChar->GetCharacterMovement()->StopMovementImmediately();
			}
          
			FVector RandomOffset = FVector(FMath::RandRange(-50.f, 50.f), FMath::RandRange(-50.f, 50.f), 50.f);
			BaseChar->TeleportTo(SafeLandingLocation + RandomOffset, BaseChar->GetActorRotation());
		}
	} 
	//  반복문 종료 
	// 사람이 없어도 아래 코드가 실행

	// =========================================================
	// 2. [보트 리셋] 
	// =========================================================

	// 2. [상태 초기화]
	SetRideState(false); 

	// 3. [물리 초기화] 속도 0으로
	if (MeshComp)
	{
		MeshComp->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
		MeshComp->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
       
		// (팁) 물리 액터를 강제 이동시킬 땐 깨워주는 게 안전함
		MeshComp->WakeAllRigidBodies(); 
	}

	// 4. [보트 원위치] 텔레포트
	SetActorLocation(InitialLocation, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorRotation(InitialRotation, ETeleportType::TeleportPhysics);

	// 5. [동기화] 강제 업데이트
	ForceNetUpdate();
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

