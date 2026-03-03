#include "BossPlatform.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
// 🚨 [필수 추가] 네트워크 동기화를 위한 헤더
#include "Net/UnrealNetwork.h" 

ABossPlatform::ABossPlatform()
{
	PrimaryActorTick.bCanEverTick = true;

	// 🚨 [핵심] 액터 자체를 네트워크에 복제하도록 켭니다.
	bReplicates = true;
	// 타임라인을 각자 재생할 것이므로 SetReplicateMovement(위치 강제 동기화)는 끕니다.

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(MeshComp);
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ABossPlatform::OnOverlapBegin);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ABossPlatform::OnOverlapEnd);

	MoveTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("MoveTimeline"));
}

// 🚨 [필수 추가] 복제할 변수 등록
void ABossPlatform::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABossPlatform, bIsDown);
	DOREPLIFETIME(ABossPlatform, bIsPermanentlyDown);
}

void ABossPlatform::BeginPlay()
{
	Super::BeginPlay();
	InitialLocation = GetActorLocation();

	if (MeshComp) DynMaterial = MeshComp->CreateAndSetMaterialInstanceDynamic(0);

	if (MovementCurve)
	{
		FOnTimelineFloat ProgressUpdate;
		ProgressUpdate.BindUFunction(this, FName("HandleProgress"));

		FOnTimelineEvent FinishedEvent;
		FinishedEvent.BindUFunction(this, FName("OnMoveFinished"));

		MoveTimeline->AddInterpFloat(MovementCurve, ProgressUpdate);
		MoveTimeline->SetTimelineFinishedFunc(FinishedEvent);
	}
}

void ABossPlatform::HandleProgress(float Value)
{
	FVector TargetLoc = FMath::Lerp(InitialLocation, InitialLocation + FVector(0, 0, DownDepth), Value);
	FVector ShakeOffset = FMath::VRand() * ShakeIntensity;
	SetActorLocation(TargetLoc + ShakeOffset);
}

void ABossPlatform::OnMoveFinished()
{
	FVector FinalLoc;
	if (bIsDown || bIsPermanentlyDown)
	{
		FinalLoc = InitialLocation + FVector(0, 0, DownDepth);
	}
	else
	{
		FinalLoc = InitialLocation;
	}
	SetActorLocation(FinalLoc);
}

// =========================================================================
// [네트워크 로직] 래퍼(Wrapper) 함수와 멀티캐스트 구현부
// =========================================================================

void ABossPlatform::SetWarning(bool bActive)
{
	if (HasAuthority()) // 서버에서만 호출 가능
	{
		Multicast_SetWarning(bActive);
	}
}

void ABossPlatform::Multicast_SetWarning_Implementation(bool bActive)
{
	if (DynMaterial)
	{
		DynMaterial->SetScalarParameterValue(FName("WarningAlpha"), bActive ? 1.0f : 0.0f);
	}
}

void ABossPlatform::MoveDown(bool bPermanent)
{
	if (HasAuthority()) // 서버에서만 실행
	{
		Multicast_MoveDown(bPermanent);

		// 타이머는 서버에서 '한 번만' 돌아가야 하므로 Multicast 바깥에 둡니다.
		if (!bPermanent)
		{
			FTimerHandle ReturnTimer;
			GetWorldTimerManager().SetTimer(ReturnTimer, this, &ABossPlatform::MoveUp, 7.0f, false);
		}
	}
}

void ABossPlatform::Multicast_MoveDown_Implementation(bool bPermanent)
{
	if (bIsPermanentlyDown) return;

	bIsDown = true;
	if (bPermanent) bIsPermanentlyDown = true;

	if (DynMaterial) DynMaterial->SetScalarParameterValue(FName("WarningAlpha"), 0.0f);

	MoveTimeline->SetPlayRate(2.0f);
	MoveTimeline->Play(); // 모든 클라이언트가 자기 컴퓨터에서 타임라인 재생!
}

void ABossPlatform::MoveUp()
{
	if (HasAuthority())
	{
		Multicast_MoveUp();
	}
}

void ABossPlatform::Multicast_MoveUp_Implementation()
{
	if (bIsPermanentlyDown) return;

	bIsDown = false;
	MoveTimeline->SetPlayRate(1.0f);
	MoveTimeline->Reverse(); // 모든 클라이언트 동시 상승!
}

// =========================================================================
// [기타 로직]
// =========================================================================

void ABossPlatform::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 오버랩 처리는 보통 서버에서만 판정하는 것이 안전합니다.
	if (HasAuthority() && OtherActor && OtherActor->IsA(ACharacter::StaticClass()))
		OverlappingPlayers.Add(OtherActor);
}

void ABossPlatform::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (HasAuthority() && OtherActor)
		OverlappingPlayers.Remove(OtherActor);
}