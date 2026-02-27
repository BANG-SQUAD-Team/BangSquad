// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/UI/Enemy/ADQTEWidget.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UADQTEWidget::NativeConstruct()
{
	Super::NativeConstruct();
	StartImageFlashing();
}

void UADQTEWidget::UpdateProgressBar(int32 Current, int32 Max)
{
	if (ADQTEProgressBar && Max > 0)
	{
		// 비율 계산 (0.0 ~ 1.0)
		float ProgressRatio = static_cast<float>(Current) / static_cast<float>(Max);

		// 프로그레스 바 수치 설정
		ADQTEProgressBar->SetPercent(ProgressRatio);
	}
}

void UADQTEWidget::StartImageFlashing()
{
	// 0.3초마다 ToggleImage 함수를 반복 실행
	GetWorld()->GetTimerManager().SetTimer(FlashTimerHandle, this, &UADQTEWidget::ToggleImage, 0.15f, true);
}

void UADQTEWidget::ToggleImage()
{
	if (!ADQTEKeyImage) return;

	// 현재 보여줄 텍스처 결정
	UTexture2D* NewTexture = bIsShowingA ? Texture_A : Texture_D;

	if (NewTexture)
	{
		ADQTEKeyImage->SetBrushFromTexture(NewTexture);
	}

	// 상태 반전
	bIsShowingA = !bIsShowingA;
}

void UADQTEWidget::StopImageFlashing()
{
	GetWorld()->GetTimerManager().ClearTimer(FlashTimerHandle);
}