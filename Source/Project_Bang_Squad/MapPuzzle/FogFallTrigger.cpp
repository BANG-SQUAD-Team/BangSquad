#include "FogFallTrigger.h"
#include "Components/BoxComponent.h"
#include "Components/LightComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "GameFramework/Character.h"

AFogFallTrigger::AFogFallTrigger()
{
	PrimaryActorTick.bCanEverTick = true; // 타임라인은 틱이 필요합니다

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	TriggerBox->SetBoxExtent(FVector(200.f, 200.f, 100.f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));

	FogTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("FogTimeline"));
}

void AFogFallTrigger::BeginPlay()
{
	Super::BeginPlay();

	// 오버랩 이벤트 바인딩
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AFogFallTrigger::OnOverlap);

	// 타임라인 세팅 (커브가 있을 때만)
	if (FogCurve)
	{
		FOnTimelineFloat TimelineCallback;

		// "UpdateFogHeight" 함수랑 타임라인을 연결
		TimelineCallback.BindUFunction(this, FName("UpdateFogHeight"));

		// 커브 에셋을 타임라인에 태움
		FogTimeline->AddInterpFloat(FogCurve, TimelineCallback);
	}

	if (TargetLight)
	{
		// GetLightComponent()는 조명의 실제 밝기 속성을 가진 컴포넌트를 가져옴
		ULightComponent* LightComp = TargetLight->GetLightComponent();
		if (LightComp)
		{
			StartIntensity = LightComp->Intensity;
		}
	}
}

void AFogFallTrigger::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 캐릭터가 닿았고, 포그 설정이 되어있다면
	if (OtherActor && OtherActor->IsA(ACharacter::StaticClass()) && TargetFog)
	{
		// 1. 현재 포그의 높이를 시작점으로 저장
		StartZHeight = TargetFog->GetActorLocation().Z;

		// 2. 타임라인 재생 (PlayFromStart: 처음부터 재생)
		FogTimeline->PlayFromStart();

		// (선택) 한 번만 작동하게 하려면 콜리전을 꺼버립니다.
		TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

// 타임라인이 도는 동안 매 프레임 호출됨 (Value는 커브의 0~1 값)
void AFogFallTrigger::UpdateFogHeight(float Value)
{
	if (TargetFog)
	{
		// Lerp(선형 보간): Start에서 Target으로 Value(0~1)만큼 이동
		float NewZ = FMath::Lerp(StartZHeight, TargetZHeight, Value);

		// X, Y는 유지하고 Z만 바꿈
		FVector CurrentLoc = TargetFog->GetActorLocation();
		CurrentLoc.Z = NewZ;

		TargetFog->SetActorLocation(CurrentLoc);
	}

	if (TargetLight)
	{
		ULightComponent* LightComp = TargetLight->GetLightComponent();
		if (LightComp)
		{
			// 시작 밝기에서 목표 밝기로 부드럽게 변환 (Lerp)
			float NewIntensity = FMath::Lerp(StartIntensity, TargetIntensity, Value);

			// 적용
			LightComp->SetIntensity(NewIntensity);
		}
	}
}