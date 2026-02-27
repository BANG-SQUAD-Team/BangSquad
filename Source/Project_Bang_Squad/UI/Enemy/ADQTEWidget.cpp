// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/UI/Enemy/ADQTEWidget.h"
#include "Components/ProgressBar.h"

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
