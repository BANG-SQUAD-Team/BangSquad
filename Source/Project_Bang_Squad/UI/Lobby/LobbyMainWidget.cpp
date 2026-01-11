// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyMainWidget.h"

#include "LobbyPlayerRow.h"
#include "Components/Button.h"
#include "Project_Bang_Squad/Game/Lobby/LobbyPlayerController.h"
#include "Project_Bang_Squad/Game/Lobby/LobbyPlayerState.h"

void ULobbyMainWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_SelectTitan) Btn_SelectTitan->OnClicked.AddDynamic(this, &ULobbyMainWidget::OnBtn_SelectTitanClicked);
	if (Btn_SelectStriker) Btn_SelectStriker->OnClicked.AddDynamic(this, &ULobbyMainWidget::OnBtn_SelectStrikerClicked);
	if (Btn_SelectMage) Btn_SelectMage->OnClicked.AddDynamic(this, &ULobbyMainWidget::OnBtn_SelectMageClicked);
	if (Btn_SelectDefender) Btn_SelectDefender->OnClicked.AddDynamic(this, &ULobbyMainWidget::OnBtn_SelectDefenderClicked);
	if (Btn_Ready) Btn_Ready->OnClicked.AddDynamic(this, &ULobbyMainWidget::OnBtn_ReadyClicked);
}

void ULobbyMainWidget::UpdatePlayerList()
{
	if (!PlayerListContainer || !PlayerRowClass) return;

	PlayerListContainer->ClearChildren();

	ALobbyGameState* GS = GetWorld()->GetGameState<ALobbyGameState>();
	if (!GS) return;

	//List 갱신
	for (APlayerState* PS : GS->PlayerArray)
	{
		ALobbyPlayerState* LobbyPS = Cast<ALobbyPlayerState>(PS);
		if (LobbyPS)
		{
			ULobbyPlayerRow* Row = CreateWidget<ULobbyPlayerRow>(PlayerListContainer, PlayerRowClass);
			if (Row)
			{
				Row->UpdateInfo(LobbyPS->GetPlayerName(), LobbyPS->bIsReady, LobbyPS->CurrentJob);
				PlayerListContainer->AddChild(Row);
			}
		}
	}

	if (ALobbyPlayerController* PC = GetLobbyPC())
	{
		if (ALobbyPlayerState* LobbyPS = PC->GetPlayerState<ALobbyPlayerState>())
		{
			if (Txt_ReadyState)
			{
				if (LobbyPS->bIsReady)
				{
					Txt_ReadyState->SetText(FText::FromString("READY"));
					Txt_ReadyState->SetColorAndOpacity(FLinearColor::Green);
				}
				else
				{
					Txt_ReadyState->SetText(FText::FromString(TEXT("NOT READY")));
					Txt_ReadyState->SetColorAndOpacity(FLinearColor::Red);
				}
			}
		}
	}
}

void ULobbyMainWidget::SetMenuVisibility(bool bVisible)
{
	if (MenuControlArea)
	{
		MenuControlArea->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
}

void ULobbyMainWidget::OnBtn_SelectTitanClicked()
{
	SelectJobClickedHelper(EJobType::Titan);
}

void ULobbyMainWidget::OnBtn_SelectStrikerClicked()
{
	SelectJobClickedHelper(EJobType::Striker);
}

void ULobbyMainWidget::OnBtn_SelectMageClicked()
{
	SelectJobClickedHelper(EJobType::Mage);
}

void ULobbyMainWidget::OnBtn_SelectDefenderClicked()
{
	SelectJobClickedHelper(EJobType::Defender);
}

void ULobbyMainWidget::OnBtn_ReadyClicked()
{
	if (auto PC = GetLobbyPC())
	{
		PC->RequestToggleReady();
	}
}

void ULobbyMainWidget::SelectJobClickedHelper(EJobType Job)
{
	if (auto PC = GetLobbyPC())
		PC->RequestChangePreviewJob(Job);
}

class ALobbyPlayerController* ULobbyMainWidget::GetLobbyPC()
{
	return Cast<ALobbyPlayerController>(GetOwningPlayer());
}
