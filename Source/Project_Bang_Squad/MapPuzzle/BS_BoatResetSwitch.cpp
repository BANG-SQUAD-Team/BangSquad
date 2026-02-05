
#include "Project_Bang_Squad/MapPuzzle/BS_BoatResetSwitch.h"
#include "Project_Bang_Squad/Character/Player/Mage/MagicBoat.h"
#include "Net/UnrealNetwork.h"

ABS_BoatResetSwitch::ABS_BoatResetSwitch()
{
 	
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	
	// 1. 본체 메쉬 (여기에 충돌체 / 박스 등을 넣어서 상호작용 감지
	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	RootComponent = BaseMesh;

	// 2. 플레이어 도착 지점 생성
	PlayerSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("PlayerSpawnPoint"));
	PlayerSpawnPoint->SetupAttachment(RootComponent);
	PlayerSpawnPoint->SetRelativeLocation(FVector(200.f, 0.f, 0.f));
}

void ABS_BoatResetSwitch::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABS_BoatResetSwitch, bIsPressed);
}

// F키 눌렀을 때 실행
void ABS_BoatResetSwitch::Interact_Implementation(APawn* InstigatorPawn)
{
	// 서버만 실행 & 쿨타임 중이면 무시
	if (!HasAuthority() || bIsPressed) return;
	
	// 1. 상태 변경 (쿨타임 시작)
	bIsPressed = true;
	OnRep_IsPressed();
	
	// 2. 보트 리셋 명령
	if (TargetBoat)
	{
		TargetBoat->ResetToStart(PlayerSpawnPoint->GetComponentLocation());
	}
	
	// 3. 2초 후 재사용 가능하도록 초기화
	GetWorldTimerManager().SetTimer(ButtonResetTimer, this, &ABS_BoatResetSwitch::ResetButtonState
		,2.0f,false);
}

void ABS_BoatResetSwitch::ResetButtonState()
{
	bIsPressed = false;
	OnRep_IsPressed();
}

void ABS_BoatResetSwitch::OnRep_IsPressed()
{
	// 나중에 UI 변경 필요할 시 여기서 처리
}