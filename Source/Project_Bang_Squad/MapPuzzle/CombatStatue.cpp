#include "Project_Bang_Squad/MapPuzzle/CombatStatue.h"
#include "Project_Bang_Squad/Character/Enemy/EnemySpawner.h"
#include "Project_Bang_Squad/MapPuzzle/CenterStatueManager.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TimelineComponent.h"
#include "Curves/CurveFloat.h"

ACombatStatue::ACombatStatue()
{
	PrimaryActorTick.bCanEverTick = true;

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
}

void ACombatStatue::BeginPlay()
{
	Super::BeginPlay();

	// 다이내믹 머터리얼 생성 (색/밝기 변경을 위해 필수)
	if (StatueMesh)
	{
		// 0번 슬롯의 머터리얼을 복사해서 다이내믹으로 만듦
		DynMaterial = StatueMesh->CreateAndSetMaterialInstanceDynamic(0);
	}

	// 트리거 바인딩
	if (TriggerSphere)
	{
		TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &ACombatStatue::OnTriggerOverlap);
	}

	// 스포너 이벤트 바인딩
	if (LinkedSpawner)
	{
		// 스포너가 끝났을 때 내 함수(OnCombatFinished)를 실행해라
		LinkedSpawner->OnSpawnerCleared.AddDynamic(this, &ACombatStatue::OnCombatFinished);
	}

	// 타임라인 설정
	if (ChangeCurve)
	{
		FOnTimelineFloat ProgressFunction;
		ProgressFunction.BindDynamic(this, &ACombatStatue::HandleTimelineProgress);
		MaterialTimeline->AddInterpFloat(ChangeCurve, ProgressFunction);

		FOnTimelineEvent FinishFunction;
		FinishFunction.BindDynamic(this, &ACombatStatue::OnTimelineFinished);
		MaterialTimeline->SetTimelineFinishedFunc(FinishFunction);
	}
}

void ACombatStatue::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// 타임라인 컴포넌트는 Tick이 필요하지 않지만 안전을 위해 호출
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
	if (bIsActivated) return;

	bIsActivated = true;

	// 1. 머터리얼 변화 시작 (서서히)
	if (MaterialTimeline)
	{
		MaterialTimeline->PlayFromStart();
	}

	// 2. 중앙 석상에게 알림
	if (CenterStatue)
	{
	    CenterStatue->ActivateLeftGoblet();
	}

	// 로그로 확인
	UE_LOG(LogTemp, Warning, TEXT("Left Statue Activated! Combat Clear!"));
}

void ACombatStatue::HandleTimelineProgress(float Value)
{
	if (DynMaterial)
	{
		// 머터리얼의 파라미터 값을 변경 (예: EmissivePower를 0에서 50으로)
		// 실제 머터리얼에서 파라미터를 ScalarParameter로 만들어둬야 함
		float TargetValue = FMath::Lerp(0.0f, 50.0f, Value);
		DynMaterial->SetScalarParameterValue(MaterialParamName, TargetValue);
	}
}

void ACombatStatue::OnTimelineFinished()
{
	// 타임라인 끝났을 때 추가 효과가 필요하면 작성
}