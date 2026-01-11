// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/Game/Lobby/LobbyPlayerController.h"

#include "InputTriggers.h"
#include "LobbyGameMode.h"
#include "LobbyPlayerState.h"
#include "Blueprint/UserWidget.h"
#include "Project_Bang_Squad/UI/Lobby/JobSelectWidget.h"
#include "Project_Bang_Squad/UI/Lobby/LobbyMainWidget.h"

void ALobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController())
	{
		if (LobbyMainWidgetClass)
		{
			LobbyMainWidget = CreateWidget<ULobbyMainWidget>(this, LobbyMainWidgetClass);
			if (LobbyMainWidget)
			{
				LobbyMainWidget->AddToViewport();

				bShowMouseCursor = true;
				SetInputMode(FInputModeGameAndUI());
			}
		}
		//GameState 복제 안됐을 수도 있어서 Timer씀
		GetWorld()->GetTimerManager().SetTimer(InitTimerHandle, this, &ALobbyPlayerController::InitLobbyUI, 0.2f, true);

		if (UBSGameInstance* GI = Cast<UBSGameInstance>(GetGameInstance()))
		{
			if (!GI->UserNickname.IsEmpty())
			{
				ServerSetNickname(GI->UserNickname);
			}
		}
	}
}

void ALobbyPlayerController::RequestChangePreviewJob(EJobType NewJob)
{
	ServerPreviewJob(NewJob);
}

void ALobbyPlayerController::RequestToggleReady()
{
	ServerToggleReady();
}

void ALobbyPlayerController::RequestConfirmedJob(EJobType FinalJob)
{
	if (UBSGameInstance* GI = Cast<UBSGameInstance>(GetGameInstance()))
	{
		GI->SetMyJob(FinalJob);
		ServerConfirmedJob(FinalJob);
	}}

void ALobbyPlayerController::RefreshLobbyUI()
{
	if (LobbyMainWidget && LobbyMainWidget->IsInViewport())
	{
		LobbyMainWidget->UpdatePlayerList();
	}

	if (JobSelectWidget && JobSelectWidget->IsInViewport())
	{
		JobSelectWidget->UpdateJobAvailAbility();
	}
}

void ALobbyPlayerController::ServerSetNickname_Implementation(const FString& NewName)
{
	if (APlayerState* PS = GetPlayerState<APlayerState>())
	{
		PS->SetPlayerName(NewName);

		UE_LOG(LogTemp, Warning, TEXT("[Server] 닉네임 변경 완료: %s"), *NewName);
	}
}

void ALobbyPlayerController::InitLobbyUI()
{
	ALobbyGameState* GS = GetWorld()->GetGameState<ALobbyGameState>();
	if (GS)
	{
		GetWorld()->GetTimerManager().ClearTimer(InitTimerHandle);

		GS->OnLobbyPhaseChanged.AddDynamic(this, &ALobbyPlayerController::OnLobbyPhaseChanged);

		OnLobbyPhaseChanged(GS->CurrentPhase);
	}
}

void ALobbyPlayerController::OnLobbyPhaseChanged(ELobbyPhase NewPhase)
{
	if (NewPhase == ELobbyPhase::PreviewJob)
	{
		if (JobSelectWidget) //혹시 몰라서 넣어둠
			JobSelectWidget->SetVisibility(ESlateVisibility::Hidden);

		SetMenuState(false);
	}
	else if (NewPhase == ELobbyPhase::SelectJob)
	{
		if (LobbyMainWidget)
			LobbyMainWidget->SetVisibility(ESlateVisibility::Hidden);

		if (!JobSelectWidget && JobSelectWidgetClass)
		{
			JobSelectWidget = CreateWidget<UJobSelectWidget>(this, JobSelectWidgetClass);
		}

		if (JobSelectWidget)
		{
			JobSelectWidget->SetVisibility(ESlateVisibility::Visible);
			JobSelectWidget->StartUp();

			JobSelectWidget->UpdateJobAvailAbility();
		}
	}
}

//TODO: 나중에 삭제 가능 / 일단 테스트를 위해 Tab키 지정함
void ALobbyPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	InputComponent->BindAction("TabMenu", IE_Pressed, this, &ALobbyPlayerController::ToggleLobbyMenu);
}

void ALobbyPlayerController::ToggleLobbyMenu()
{
	SetMenuState(!bIsMenuVisible);
}

void ALobbyPlayerController::SetMenuState(bool bShow)
{
	bIsMenuVisible = bShow;

	//UI보이기 / 숨기기
	if (LobbyMainWidget)
	{
		LobbyMainWidget->SetVisibility(ESlateVisibility::Visible);
		LobbyMainWidget->SetMenuVisibility(bShow);
		
	}

	//입력 모드 전환
	if (bShow)
	{
		FInputModeGameAndUI InputMode;
		if (LobbyMainWidget)
			InputMode.SetWidgetToFocus(LobbyMainWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
		bShowMouseCursor = true;

		if (LobbyMainWidget)
			LobbyMainWidget->UpdatePlayerList();
	}
	else
	{
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = false;
	}
}

void ALobbyPlayerController::ServerPreviewJob_Implementation(EJobType NewJob)
{
	UE_LOG(LogTemp, Warning, TEXT("[Server] PC: PreviewJob 요청 받음! (JobIndex: %d)"), (uint8)NewJob);
	
	if (ALobbyPlayerState* PS = GetPlayerState<ALobbyPlayerState>())
	{
		PS->SetJob(NewJob);
	}

	if (ALobbyGameMode* GM = GetWorld()->GetAuthGameMode<ALobbyGameMode>())
	{
		GM->ChangePlayerCharacter(this, NewJob);
	}
}

void ALobbyPlayerController::ServerToggleReady_Implementation()
{
	UE_LOG(LogTemp, Warning, TEXT("[Server] PC: Ready 토글 요청 받음!"));
	
	if (ALobbyPlayerState* PS = GetPlayerState<ALobbyPlayerState>())
	{
		PS->SetIsReady(!PS->bIsReady);

		if (ALobbyGameMode* GM = GetWorld()->GetAuthGameMode<ALobbyGameMode>())
		{
			GM->CheckAllReady();
		}
	}
}

void ALobbyPlayerController::ServerConfirmedJob_Implementation(EJobType FinalJob)
{
	UE_LOG(LogTemp, Warning, TEXT("[Server] PC: Job Confirm 요청 받음! (JobIndex: %d)"), (uint8)FinalJob);

	ALobbyPlayerState* PS = GetPlayerState<ALobbyPlayerState>();
	ALobbyGameMode* GM = GetWorld()->GetAuthGameMode<ALobbyGameMode>();
	
	if (PS && GM)
	{
		if (GM->IsJobTaken(FinalJob, PS))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Server] 거절됨: 이미 선택된 직업"));
			return;
		}

		//직업 설정 및 확정 상태로 변경
		PS->SetJob(FinalJob);
		PS->SetIsConfirmedJob(true);

		//게임 시작 조건 체크
		GM->CheckConfirmedJob();
	}
}
