// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/UI/Lobby/JobSelectWidget.h"

#include "Components/Button.h"
#include "Project_Bang_Squad/Game/Lobby/LobbyPlayerController.h"

void UJobSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_SelectTitan) Btn_SelectTitan->OnClicked.AddDynamic(this, &UJobSelectWidget::OnPickTitan);
	if (Btn_SelectStriker) Btn_SelectStriker->OnClicked.AddDynamic(this, &UJobSelectWidget::OnPickStriker);
	if (Btn_SelectMage) Btn_SelectMage->OnClicked.AddDynamic(this, &UJobSelectWidget::OnPickMage);
	if (Btn_SelectDefender) Btn_SelectDefender->OnClicked.AddDynamic(this, &UJobSelectWidget::OnPickDefender);
	if (Btn_Confirm) Btn_Confirm->OnClicked.AddDynamic(this, &UJobSelectWidget::OnConfirmClicked);
}

void UJobSelectWidget::OnPickTitan()
{
	PendingJob = EJobType::Titan;
}

void UJobSelectWidget::OnPickStriker()
{
	PendingJob = EJobType::Striker;
}

void UJobSelectWidget::OnPickMage()
{
	PendingJob = EJobType::Mage;
}

void UJobSelectWidget::OnPickDefender()
{
	PendingJob = EJobType::Defender;
}

void UJobSelectWidget::OnConfirmClicked()
{
	if (PendingJob == EJobType::None) return;

	ALobbyPlayerController* PC = Cast<ALobbyPlayerController>(GetOwningPlayer());
	if (PC)
	{
		PC->RequestConfirmedJob(PendingJob);

		if (Btn_Confirm)
			Btn_Confirm->SetIsEnabled(false);
	}
}
