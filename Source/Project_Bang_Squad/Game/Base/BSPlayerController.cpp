#include "Project_Bang_Squad/Game/Base/BSPlayerController.h"
#include "Project_Bang_Squad/Game/Base/BSPlayerState.h"
#include "Project_Bang_Squad/Core/BSGameInstance.h"
#include "Blueprint/UserWidget.h"

// [QTE System Includes]
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Project_Bang_Squad/UI/Stage/Boss/QTEWidget.h"
#include "Project_Bang_Squad/Character/StageBoss/Stage2Boss.h"

ABSPlayerController::ABSPlayerController()
{
	// 기본값 초기화
	bIsQTEActive = false;
	CurrentTapCount = 0;
	GoalTapCount = 0;
}

void ABSPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// [Network] 로컬 플레이어인 경우 직업 정보를 서버에 동기화
	if (IsLocalController())
	{
		if (UBSGameInstance* GI = GetGameInstance<UBSGameInstance>())
		{
			EJobType MyJob = GI->GetPlayerJob();
			if (ABSPlayerState* PS = GetPlayerState<ABSPlayerState>())
			{
				PS->Server_SetJob(MyJob);
			}
		}
	}
}

void ABSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// 1. 관리 중인 일반 위젯 정리
	for (TObjectPtr<UUserWidget> Widget : ManagedWidgets)
	{
		if (Widget && Widget->IsInViewport())
		{
			Widget->RemoveFromParent();
		}
	}
	ManagedWidgets.Empty();

	// 2. QTE 위젯 정리 (맵 이동 시 잔존 방지)
	if (QTEWidgetInstance && QTEWidgetInstance->IsInViewport())
	{
		QTEWidgetInstance->RemoveFromParent();
		QTEWidgetInstance = nullptr;
	}
}

void ABSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// 1. [Enhanced Input] 게임플레이 및 QTE 로직용
	// UE 5.5 표준: CastChecked 대신 안전한 Cast 사용 권장
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// QTE 연타 액션 바인딩 (Start Trigger)
		if (QTE_TapAction)
		{
			EnhancedInputComponent->BindAction(QTE_TapAction, ETriggerEvent::Started, this, &ABSPlayerController::OnQTETapInput);
		}
	}

	// 2. [Legacy Input] 디버그/치트용 (빠른 개발을 위해 기존 방식 유지)
	// F1 ~ F9 키 바인딩
	InputComponent->BindKey(EKeys::F1, IE_Pressed, this, &ABSPlayerController::MoveToStage1Map);
	InputComponent->BindKey(EKeys::F2, IE_Pressed, this, &ABSPlayerController::MoveToStage1MiniGameMap);
	InputComponent->BindKey(EKeys::F3, IE_Pressed, this, &ABSPlayerController::MoveToStage1BossMap);

	InputComponent->BindKey(EKeys::F4, IE_Pressed, this, &ABSPlayerController::MoveToStage2Map);
	InputComponent->BindKey(EKeys::F5, IE_Pressed, this, &ABSPlayerController::MoveToStage2MiniGameMap);
	InputComponent->BindKey(EKeys::F6, IE_Pressed, this, &ABSPlayerController::MoveToStage2BossMap);

	InputComponent->BindKey(EKeys::F7, IE_Pressed, this, &ABSPlayerController::MoveToStage3Map);
	InputComponent->BindKey(EKeys::F8, IE_Pressed, this, &ABSPlayerController::MoveToStage3MiniGameMap);
	InputComponent->BindKey(EKeys::F9, IE_Pressed, this, &ABSPlayerController::MoveToStage3BossMap);
}

void ABSPlayerController::RegisterManagedWidget(UUserWidget* InWidget)
{
	if (IsValid(InWidget))
	{
		ManagedWidgets.Add(InWidget);
	}
}

void ABSPlayerController::ServerSetNickName_Implementation(const FString& NewName)
{
	// [Server] PlayerState에 닉네임 전파 (Replication 담당)
	if (ABSPlayerState* PS = GetPlayerState<ABSPlayerState>())
	{
		PS->SetPlayerName(NewName);
	}
}

#pragma region Boss QTE System

void ABSPlayerController::Client_StartQTE_Implementation(AStage2Boss* BossActor, int32 TargetCount, float Duration)
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

void ABSPlayerController::OnQTETapInput()
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

void ABSPlayerController::Client_EndQTE_Implementation(bool bSuccess)
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

#pragma endregion

#pragma region Debug Moves

void ABSPlayerController::ServerDebugMoveToMapHandle_Implementation(EStageIndex Stage, EStageSection Section)
{
	// [Server] GameInstance를 통해 맵 트래블 실행
	if (UBSGameInstance* GI = GetGameInstance<UBSGameInstance>())
	{
		GI->MoveToStage(Stage, Section);
	}
}

// --- Wrapper Functions (Input Bindings) ---

void ABSPlayerController::MoveToStage1Map() { ServerDebugMoveToMapHandle(EStageIndex::Stage1, EStageSection::Main); }
void ABSPlayerController::MoveToStage1MiniGameMap() { ServerDebugMoveToMapHandle(EStageIndex::Stage1, EStageSection::MiniGame); }
void ABSPlayerController::MoveToStage1BossMap() { ServerDebugMoveToMapHandle(EStageIndex::Stage1, EStageSection::Boss); }

void ABSPlayerController::MoveToStage2Map() { ServerDebugMoveToMapHandle(EStageIndex::Stage2, EStageSection::Main); }
void ABSPlayerController::MoveToStage2MiniGameMap() { ServerDebugMoveToMapHandle(EStageIndex::Stage2, EStageSection::MiniGame); }
void ABSPlayerController::MoveToStage2BossMap() { ServerDebugMoveToMapHandle(EStageIndex::Stage2, EStageSection::Boss); }

void ABSPlayerController::MoveToStage3Map() { ServerDebugMoveToMapHandle(EStageIndex::Stage3, EStageSection::Main); }
void ABSPlayerController::MoveToStage3MiniGameMap() { ServerDebugMoveToMapHandle(EStageIndex::Stage3, EStageSection::MiniGame); }
void ABSPlayerController::MoveToStage3BossMap() { ServerDebugMoveToMapHandle(EStageIndex::Stage3, EStageSection::Boss); }

#pragma endregion