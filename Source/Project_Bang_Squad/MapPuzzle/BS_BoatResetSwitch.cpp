
#include "Project_Bang_Squad/MapPuzzle/BS_BoatResetSwitch.h"
#include "Project_Bang_Squad/Character/Player/Mage/MagicBoat.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Pawn.h"
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
	
	// 3. 상호작용 콜리전
	InteractSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractSphere"));
	InteractSphere->SetupAttachment(RootComponent);
	InteractSphere->InitSphereRadius(150.0f);
	InteractSphere->SetCollisionProfileName(TEXT("Trigger"));

	// 4. 상호작용 UI 위젯
	InteractWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractWidget"));
	InteractWidget->SetupAttachment(RootComponent);
	InteractWidget->SetRelativeLocation(FVector(0.f, 0.f, 100.f)); 
	InteractWidget->SetWidgetSpace(EWidgetSpace::Screen); 
	InteractWidget->SetDrawSize(FVector2D(150.f, 50.f));
	InteractWidget->SetVisibility(false);
}

void ABS_BoatResetSwitch::BeginPlay()
{
	Super::BeginPlay();

	// 오버랩 이벤트 바인딩
	InteractSphere->OnComponentBeginOverlap.AddDynamic(this, &ABS_BoatResetSwitch::OnSphereBeginOverlap);
	InteractSphere->OnComponentEndOverlap.AddDynamic(this, &ABS_BoatResetSwitch::OnSphereEndOverlap);
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
	
	// 누르는 순간 호스트 화면에서 UI 즉시 숨김
	if (InteractWidget) InteractWidget->SetVisibility(false);
	
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
	if (bIsPressed && InteractWidget)
	{
		InteractWidget->SetVisibility(false);
	}
}

void ABS_BoatResetSwitch::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bIsPressed) return;
	
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (Pawn && Pawn->IsLocallyControlled())
	{
		InteractWidget->SetVisibility(true);
	}
}

void ABS_BoatResetSwitch::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (Pawn && Pawn->IsLocallyControlled())
	{
		InteractWidget->SetVisibility(false);
	}
}
