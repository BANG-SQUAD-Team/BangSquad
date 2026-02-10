#include "Project_Bang_Squad/MapPuzzle/CombatStatue.h"
#include "Project_Bang_Squad/Character/Enemy/EnemySpawner.h"
#include "Project_Bang_Squad/MapPuzzle/CenterStatueManager.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TimelineComponent.h"
#include "Net/UnrealNetwork.h"
#include "Curves/CurveFloat.h"

ACombatStatue::ACombatStatue()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// 1. 메쉬 설정
	StatueMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatueMesh"));
	RootComponent = StatueMesh;

	// 2. 감지 범위 설정 (반경 5미터)
	TriggerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerSphere"));
	TriggerSphere->SetupAttachment(RootComponent);
	TriggerSphere->SetSphereRadius(500.0f);
	TriggerSphere->SetCollisionProfileName(TEXT("Trigger"));

	// 3. 타임라인 컴포넌트
	MaterialTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("MaterialTimeline"));
	MaterialTimeline->SetIsReplicated(true);
}

void ACombatStatue::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACombatStatue, bIsActivated); // 변수 동기화 등록
}

void ACombatStatue::BeginPlay()
{
	Super::BeginPlay();

	if (StatueMesh)
	{
		DynMaterial = StatueMesh->CreateAndSetMaterialInstanceDynamic(1);
	}

	if (HasAuthority())
	{
		if (TriggerSphere)
			TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &ACombatStatue::OnTriggerOverlap);

		// [디버그] 스포너 연결 확인
		if (LinkedSpawner)
		{
			// 연결 성공 로그
			UE_LOG(LogTemp, Warning, TEXT("✅ [CombatStatue] Spawner Linked! Listening for Clear Signal..."));

			// 이벤트 바인딩
			LinkedSpawner->OnSpawnerCleared.AddDynamic(this, &ACombatStatue::OnCombatFinished);
		}
		else
		{
			// 연결 실패 로그 (빨간색 경고)
			UE_LOG(LogTemp, Error, TEXT("❌ [CombatStatue] LinkedSpawner is NULL! Please check Level Editor Details."));
		}
	}

	if (ChangeCurve)
	{
		FOnTimelineFloat ProgressFunction;
		ProgressFunction.BindDynamic(this, &ACombatStatue::HandleTimelineProgress);
		MaterialTimeline->AddInterpFloat(ChangeCurve, ProgressFunction);
	}
}

void ACombatStatue::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (MaterialTimeline) MaterialTimeline->TickComponent(DeltaTime, ELevelTick::LEVELTICK_TimeOnly, NULL);
}

void ACombatStatue::OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 이미 켜졌으면 무시
	if (bIsActivated) return;

	// 플레이어인지 확인
	if (OtherActor && OtherActor->ActorHasTag("Player"))
	{
		// 스포너 작동!
		if (LinkedSpawner)
		{
			LinkedSpawner->SetSpawnerActive(true);
		}
	}
}

void ACombatStatue::OnCombatFinished()
{
	if (!HasAuthority()) return;

	UE_LOG(LogTemp, Warning, TEXT("[CombatStatue] Clear Signal Received! Activating Statue..."));

	bIsActivated = true;
	OnRep_IsActivated();

	if (CenterStatue)
	{
		CenterStatue->ActivateLeftGoblet();
		UE_LOG(LogTemp, Warning, TEXT("[CombatStatue] Signal sent to CenterStatue."));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatStatue] CenterStatue is NULL! Cannot light the fire."));
	}
}

void ACombatStatue::OnRep_IsActivated()
{
	// [클라이언트+서버] 타임라인 재생 (시각 효과 동기화)
	if (MaterialTimeline) MaterialTimeline->PlayFromStart();
}

void ACombatStatue::HandleTimelineProgress(float Value)
{
	if (DynMaterial)
	{
		float TargetValue = FMath::Lerp(0.0f, 50.0f, Value);
		DynMaterial->SetScalarParameterValue(MaterialParamName, TargetValue);
	}
}