// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/Game/Lobby/LobbyPlayerController.h"

#include "EnhancedInputComponent.h"
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
				RegisterManagedWidget(LobbyMainWidget);
				
				SetMenuState(true);
			}
		}
		//GameState 복제 안됐을 수도 있어서 Timer씀
		//GetWorld()->GetTimerManager().SetTimer(InitTimerHandle, this, &ALobbyPlayerController::InitLobbyUI, 0.2f, true);

		InitLobbyUI();
		
		if (UBSGameInstance* GI = Cast<UBSGameInstance>(GetGameInstance()))
		{
			if (!GI->UserNickname.IsEmpty())
			{
				ServerSetNickName(GI->UserNickname);
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
		GI->SetPlayerJob(FinalJob);
		UE_LOG(LogTemp, Log, TEXT("Local GameInstance Job Saved: %d"), (int32)FinalJob);
		ServerConfirmedJob(FinalJob);
	}}

void ALobbyPlayerController::RefreshLobbyUI()
{
	if (!GetWorld() || GetWorld()->IsInSeamlessTravel())
	{
		return; 
	}
	
	ALobbyGameState* GS = GetWorld()->GetGameState<ALobbyGameState>();
	if (GS && GS->CurrentPhase == ELobbyPhase::GameStarting)
		return;
	
	if (IsValid(LobbyMainWidget) && LobbyMainWidget->IsInViewport())
	{
		LobbyMainWidget->UpdatePlayerList();
	}

	if (IsValid(JobSelectWidget) && JobSelectWidget->IsInViewport())
	{
		JobSelectWidget->UpdateJobAvailability(GS->GetTakenJobs());
	}
}

void ALobbyPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	//Timer 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InitTimerHandle);
	}
}

void ALobbyPlayerController::Client_JobSelectFailed_Implementation(EJobType FailedJob)
{
	if (JobSelectWidget)
	{
		if (ALobbyGameState* GS = GetWorld()->GetGameState<ALobbyGameState>())
		{
			JobSelectWidget->UpdateJobAvailability(GS->GetTakenJobs());
		}
	}
}

// void ALobbyPlayerController::ServerSetNickName_Implementation(const FString& NewName) 
// {
// 	// [수정] this->를 붙여서 명확하게 호출하거나, 상속받은 변수 PlayerState를 사용합니다.
// 	if (APlayerState* PS = this->GetPlayerState<APlayerState>())
// 	{
// 		PS->SetPlayerName(NewName);
// 		UE_LOG(LogTemp, Warning, TEXT("[Server] 닉네임 변경 완료: %s"), *NewName);
// 	}
// }

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
		if (JobSelectWidget) 
			JobSelectWidget->SetVisibility(ESlateVisibility::Hidden);

		SetMenuState(true);
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

			if (ALobbyGameState* GS = GetWorld()->GetGameState<ALobbyGameState>())
				JobSelectWidget->UpdateJobAvailability(GS->GetTakenJobs());
		}
	}
}

void ALobbyPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (IA_ToggleLobbyMenu)
		{
			EIC->BindAction(IA_ToggleLobbyMenu, ETriggerEvent::Started, this, &ALobbyPlayerController::ToggleLobbyMenu);
		}
	}}

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

	if (ABSGameMode* GM = GetWorld()->GetAuthGameMode<ALobbyGameMode>())
	{
		GM->SpawnPlayerCharacter(this, NewJob);
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
	ALobbyPlayerState* PS = GetPlayerState<ALobbyPlayerState>();
	ALobbyGameMode* GM = GetWorld()->GetAuthGameMode<ALobbyGameMode>();
	
	if (PS && GM)
	{
		bool bSuccess = GM->TryConfirmJob(FinalJob, PS);

		if (!bSuccess)
		{
			Client_JobSelectFailed(FinalJob);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[Server] 직업 확정 성공: %d"), (uint8)FinalJob);
		}
	}
}
