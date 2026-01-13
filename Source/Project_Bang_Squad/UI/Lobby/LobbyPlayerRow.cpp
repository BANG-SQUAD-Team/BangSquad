// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/UI/Lobby/LobbyPlayerRow.h"

void ULobbyPlayerRow::UpdateInfo(FString PlayerName, bool bIsReady, EJobType JobType)
{
	if (Txt_Name)
		Txt_Name->SetText(FText::FromString(PlayerName));

	if (Txt_ReadyState)
	{
		Txt_ReadyState->SetText(bIsReady ? FText::FromString(TEXT("READY")) : FText::FromString(TEXT("NOT READY")));
		Txt_ReadyState->SetColorAndOpacity(bIsReady ? FLinearColor::Green : FLinearColor::Red);
	}

	//TODO: JobType에 따라 직업 아이콘 바꾸기 등 추가 가능함
}
