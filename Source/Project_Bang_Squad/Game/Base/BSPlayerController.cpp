#include "Project_Bang_Squad/Game/Base/BSPlayerController.h"
#include "BSPlayerState.h"
#include "Project_Bang_Squad/Core/BSGameInstance.h"
#include "Blueprint/UserWidget.h"
#include "Project_Bang_Squad/Shop/ShopMainWidget.h" 
#include "GameFramework/Character.h"

void ABSPlayerController::BeginPlay()
{
	Super::BeginPlay();

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

	//레벨 이동이나 종료 시 관리하던 위젯들을 모두 제거
	//for (UUserWidget* Widget : ManagedWidgets)
	for (TObjectPtr<UUserWidget> Widget : ManagedWidgets)
	{
		//if (IsValid(Widget))
		if (Widget && Widget->IsInViewport())
		{
			Widget->RemoveFromParent();
		}
	}
	ManagedWidgets.Empty();
}

void ABSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	// F1 ~ F9 키
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
	//if (InWidget)
	if (IsValid(InWidget))
	{
		ManagedWidgets.Add(InWidget);
	}
}

#include "Project_Bang_Squad/Character/Base/BaseCharacter.h" // 캐릭터 헤더 포함 필수

void ABSPlayerController::ToggleShopUI()
{
	// 1. 닫기 로직 (이미 열려있으면 끄기)S
	if (ShopWidgetInstance && ShopWidgetInstance->IsInViewport())
	{
		ShopWidgetInstance->RemoveFromParent();
		bShowMouseCursor = false;
		SetInputMode(FInputModeGameOnly());

		// 닫을 때 스튜디오 카메라도 꺼주면 좋음 (선택사항)
		return;
	}

	// 2. 생성 로직
	if (!ShopWidgetInstance && ShopWidgetClass)
	{
		ShopWidgetInstance = CreateWidget<UShopMainWidget>(this, ShopWidgetClass);
	}

	if (ShopWidgetInstance)
	{
		ShopWidgetInstance->AddToViewport();

		FName JobTag = NAME_None;
		if (APawn* MyPawn = GetPawn())
		{
			// [디버그 로그] 캐릭터가 가진 모든 태그를 다 찍어봅니다.
			UE_LOG(LogTemp, Warning, TEXT("[Controller] 캐릭터 태그 총 개수: %d"), MyPawn->Tags.Num());
			for (int32 i = 0; i < MyPawn->Tags.Num(); i++)
			{
				UE_LOG(LogTemp, Log, TEXT("[Controller] 태그 %d번: %s"), i, *MyPawn->Tags[i].ToString());
			}

			// 1번 인덱스를 가져오되, 없으면 0번이라도 시도
			if (MyPawn->Tags.Num() >= 2) JobTag = MyPawn->Tags[1];
			else if (MyPawn->Tags.Num() >= 1) JobTag = MyPawn->Tags[0];
		}

		ShopWidgetInstance->InitShop(JobTag);

		// --- 마우스 및 입력 모드 설정 ---
		bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(ShopWidgetInstance->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
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