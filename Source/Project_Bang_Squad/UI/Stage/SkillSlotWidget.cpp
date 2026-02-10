// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/UI/Stage/SkillSlotWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"

void USkillSlotWidget::NativeTick(const FGeometry& MyGeoMetry, float DeltaTime)
{
	Super::NativeTick(MyGeoMetry, DeltaTime);
	
	if (bIsCoolingDown)
	{
		CurrentCooldownTime -= DeltaTime;
		
		// 1. 텍스트 갱신 (소수점 1자리)
		if (Txt_Time)
		{
			Txt_Time->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), CurrentCooldownTime)));
				
		}
		
		// 2. 게이지 갱신
		if (PB_Cooldown)
		{
			float Percent = FMath::Clamp(CurrentCooldownTime / MaxCooldownTime, 0.0f, 1.0f);
			PB_Cooldown->SetPercent(Percent);
		}
		
		// 3. 종료 체크
		if (CurrentCooldownTime <= 0.0f)
		{
			bIsCoolingDown = false;
			CurrentCooldownTime = 0.0f;
			
			// UI 원상복구
			if (Img_Dim) Img_Dim->SetVisibility(ESlateVisibility::Hidden);
			if (Txt_Time) Txt_Time->SetVisibility(ESlateVisibility::Hidden);
			if (PB_Cooldown) PB_Cooldown->SetPercent(0.0f);
		}
	}
}

void USkillSlotWidget::StartCooldown(float Duration)
{
	if (Duration <= 0.0f) return;
	
	bIsCoolingDown = true;
	MaxCooldownTime = Duration;
	CurrentCooldownTime = Duration;
	
	// UI 켜기
	if (Img_Dim) Img_Dim->SetVisibility(ESlateVisibility::Visible);
	if (Txt_Time) Txt_Time->SetVisibility(ESlateVisibility::Visible);
}

void USkillSlotWidget::SetIcon(UTexture2D* NewIcon)
{
	if (Img_Icon && NewIcon)
	{
		Img_Icon->SetBrushFromTexture(NewIcon);
	}
}

void USkillSlotWidget::SetSlotLockedState(bool bIsLocked)
{
	if (!Img_Icon) return;
	
	if (bIsLocked)
	{
		// 잠김
		Img_Icon->SetColorAndOpacity(FLinearColor(0.15f, 0.15f, 0.15f, 0.5f));
		
		// 잠긴 상태라면 쿨타임 UI들은 확실하게 숨기기
		if (Img_Dim) Img_Dim->SetVisibility(ESlateVisibility::Hidden);
		if (Txt_Time) Txt_Time->SetVisibility(ESlateVisibility::Hidden);
	}
	else
	{
		// 해금됨
		Img_Icon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
	}
}
