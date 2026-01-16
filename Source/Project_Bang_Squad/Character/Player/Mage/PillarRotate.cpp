#include "PillarRotate.h"
#include "Curves/CurveFloat.h"

APillarRotate::APillarRotate()
{
	PrimaryActorTick.bCanEverTick = false;

	// 1. 씬 컴포넌트 생성 및 루트 설정
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// 2. 기둥 메쉬 생성 및 루트에 부착
	PillarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PillarMesh"));
	PillarMesh->SetupAttachment(SceneRoot); 

	FallTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("FallTimeline"));
}

void APillarRotate::BeginPlay()
{
	Super::BeginPlay();

	InitialRotation = GetActorRotation();

	// 타임라인과 커브 연결
	if (FallCurve)
	{
		FOnTimelineFloat ProgressFunction;
		ProgressFunction.BindDynamic(this, &APillarRotate::HandleFallProgress);
		FallTimeline->AddInterpFloat(FallCurve, ProgressFunction);
	}
}

void APillarRotate::SetMageHighlight(bool bActive)
{
	// 메이지가 쳐다볼 때 아웃라인 활성화
	if (PillarMesh)
	{
		PillarMesh->SetRenderCustomDepth(bActive);
		PillarMesh->SetCustomDepthStencilValue(bActive ? 250 : 0);
	}
}

void APillarRotate::ProcessMageInput(FVector Direction)
{
	if (bIsFalling || !FallCurve) return;

	// [핵심] 마우스 위로 올리기 판정
	// MageCharacter의 Look에서 LookInput.Y(상하)가 반영된 벡터가 들어옵니다.
	// 탑다운 카메라 기준에서 상향 벡터 성분이 충분히 큰지 확인합니다.
	if (Direction.Size() > 0.3f)
	{
		// 방향 검증: 마우스를 아래가 아닌 '위'로 올렸을 때만 실행
		// (필요 시 Direction과 카메라 UpVector의 내적을 통해 더 정교하게 판정 가능)
		bIsFalling = true;
		FallTimeline->PlayFromStart();

		// 트리거 성공 시 하이라이트 종료
		SetMageHighlight(false);
	}
}

void APillarRotate::HandleFallProgress(float Value)
{
	// 쿼터니언을 이용한 축 기준 회전 계산
	// Value는 커브에 설정된 값 (예: 1.0 도달 시 90도 회전)
	FQuat DeltaRotation = FQuat(RotationAxis, FMath::DegreesToRadians(MaxFallAngle * Value));

	// 초기 회전에 변화량을 곱하여 회전 적용
	SetActorRotation(DeltaRotation * InitialRotation.Quaternion());
}