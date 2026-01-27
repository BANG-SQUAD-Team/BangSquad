#include "Project_Bang_Squad/Game/MapPattern/MagicGate.h"
#include "Net/UnrealNetwork.h" // 필수 헤더

AMagicGate::AMagicGate()
{
	PrimaryActorTick.bCanEverTick = true;
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>("MeshComp");
	RootComponent = MeshComp;
    
	bReplicates = true;
    
	// 위치 동기화를 끕니다! 
	// 각자(서버/클라) Tick에서 계산해서 움직일 것이므로, 서버가 좌표를 강제하면 안 됩니다.
	SetReplicateMovement(false); 
}

void AMagicGate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// bIsOpen 변수를 네트워크 동기화 등록
	DOREPLIFETIME(AMagicGate, bIsOpen);
}

void AMagicGate::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
    
	// bIsOpen이 true면 서버든 클라든 각자 알아서 부드럽게 내립니다.
	if (bIsOpen)
	{
		FVector CurrentLoc = GetActorLocation();
		// 목표 지점까지 부드럽게 (InterpSpeed 조절 가능)
		float NewZ = FMath::FInterpTo(CurrentLoc.Z, TargetZ, DeltaTime, 2.0f);
       
		SetActorLocation(FVector(CurrentLoc.X, CurrentLoc.Y, NewZ));
	}
}

void AMagicGate::OpenGate()
{
	// 서버에서만 실행
	if (HasAuthority())
	{
		if (!bIsOpen)
		{
			bIsOpen = true;
			// 서버에서도 로직 실행을 위해 함수 직접 호출
			OnRep_IsOpen(); 
		}
	}
}

// 서버에서 bIsOpen이 true가 되면, 클라이언트들도 이 함수가 자동 실행됨
void AMagicGate::OnRep_IsOpen()
{
	if (bIsOpen)
	{
		// 목표 위치 계산 (현재 위치 기준)
		TargetZ = GetActorLocation().Z - OpenDistance;
        
		// (선택) 여기서 문 열리는 소리나 이펙트를 재생하면 완벽합니다.
		// UGameplayStatics::PlaySoundAtLocation(...)
	}
}