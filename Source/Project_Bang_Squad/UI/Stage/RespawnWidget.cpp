// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/UI/Stage/RespawnWidget.h"

#include "Components/TextBlock.h"

void URespawnWidget::StartCountdown(float Duration)
{
	LeftTime = Duration;
	bIsCounting = true;

	if (CountdownText)
	{
		CountdownText->SetText(FText::AsNumber(FMath::CeilToInt(LeftTime)));
	}
}

void URespawnWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bIsCounting && LeftTime > 0.f)
	{
		LeftTime -= InDeltaTime;

		if (CountdownText)
		{
			CountdownText->SetText(FText::AsNumber(FMath::CeilToInt(LeftTime)));
		}
	}

	else if (LeftTime <= 0.f)
	{
		bIsCounting = false;
	}
}
