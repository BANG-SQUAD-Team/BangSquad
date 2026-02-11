// Fill out your copyright notice in the Description page of Project Settings.


#include "Stage2BossPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Stage2Boss.h"
#include "Project_Bang_Squad/Character/StageBoss/Stage2Boss.h"
#include "Project_Bang_Squad/UI/Stage/Boss/QTEWidget.h"

AStage2BossPlayerController::AStage2BossPlayerController()
{
	// 기본값 초기화
	bIsQTEActive = false;
	CurrentTapCount = 0;
	GoalTapCount = 0;
}

void AStage2BossPlayerController::BeginPlay()
{
	Super::BeginPlay();
}

void AStage2BossPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// 2. QTE 위젯 정리 (맵 이동 시 잔존 방지)
	if (QTEWidgetInstance && QTEWidgetInstance->IsInViewport())
	{
		QTEWidgetInstance->RemoveFromParent();
		QTEWidgetInstance = nullptr;
	}
}

void AStage2BossPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// 1. [Enhanced Input] 게임플레이 및 QTE 로직용
	// UE 5.5 표준: CastChecked 대신 안전한 Cast 사용 권장
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// QTE 연타 액션 바인딩 (Start Trigger)
		if (QTE_TapAction)
		{
			EnhancedInputComponent->BindAction(QTE_TapAction, ETriggerEvent::Started, this, &AStage2BossPlayerController::OnQTETapInput);
		}
	}
}
#pragma region Boss QTE System

void AStage2BossPlayerController::Client_StartQTE_Implementation(AStage2Boss* BossActor, int32 TargetCount,
	float Duration)
{
	// 로컬 컨트롤러가 아니면 UI나 입력을 처리할 필요 없음
	if (!IsLocalController()) return;

	// 1. 상태 초기화
	CurrentBossRef = BossActor;
	GoalTapCount = TargetCount;
	CurrentTapCount = 0;
	bIsQTEActive = true;

	// 2. 입력 컨텍스트(IMC) 교체 (Priority 100으로 설정하여 이동 키 덮어씀)
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (QTE_Context)
		{
			Subsystem->AddMappingContext(QTE_Context, 100);
		}
	}

	// 3. QTE 전용 UI 생성 및 표시
	if (QTEWidgetClass)
	{
		// 기존 위젯 안전 제거
		if (QTEWidgetInstance)
		{
			QTEWidgetInstance->RemoveFromParent();
			QTEWidgetInstance = nullptr;
		}

		QTEWidgetInstance = CreateWidget<UQTEWidget>(this, QTEWidgetClass);
		if (QTEWidgetInstance)
		{
			QTEWidgetInstance->AddToViewport(100); // UI 최상단 표시
			// TODO: QTEWidgetInstance->SetDuration(Duration); // 위젯에 시간 정보 전달 가능
		}
	}
}

void AStage2BossPlayerController::Client_EndQTE_Implementation(bool bSuccess)
{
	bIsQTEActive = false;

	// 1. 입력 컨텍스트 제거 (일반 이동 복구)
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (QTE_Context)
		{
			Subsystem->RemoveMappingContext(QTE_Context);
		}
	}

	// 2. UI 제거
	if (QTEWidgetInstance)
	{
		QTEWidgetInstance->RemoveFromParent();
		QTEWidgetInstance = nullptr;
	}

	// 참조 해제 (메모리 누수 방지)
	CurrentBossRef = nullptr;
}

void ABSPlayerController::Server_ReportQTEResult_Implementation(AStage2Boss* Boss, bool bSuccess)
{
	if (Boss)
	{
		// 보스에게 결과 전달 (이전에 만든 보스의 RPC 호출)
		Boss->ServerRPC_QTEResult(this, bSuccess);
	}
}

void AStage2BossPlayerController::OnQTETapInput()
{
	if (!bIsQTEActive) return;

	CurrentTapCount++;

	// TODO: 위젯에 현재 연타 횟수 업데이트 (QTEWidgetInstance->UpdateProgress(CurrentTapCount, GoalTapCount))

	// 성공 조건 달성 체크
	if (CurrentTapCount >= GoalTapCount)
	{
		bIsQTEActive = false; // 중복 성공 방지

		// [Server RPC] 보스에게 성공 사실 통보
		if (CurrentBossRef)
		{
			// 주의: 보스 클래스에 ServerRPC_QTEResult 함수가 구현되어 있어야 함
			CurrentBossRef->ServerRPC_QTEResult(this, true);
		}

		// 클라이언트 측 정리 (성공 연출은 위젯 내부에서 처리 가능)
		Client_EndQTE(true);
	}
}
#pragma endregion

