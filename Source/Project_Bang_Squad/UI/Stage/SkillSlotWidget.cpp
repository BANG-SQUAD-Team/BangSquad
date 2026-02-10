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
		
		// 2. 원형 쿨타임
		if (CooldownMatDynamic)
		{
			// 0.0 ~ 1.0 비율 계산
			float Percent = FMath::Clamp(CurrentCooldownTime / MaxCooldownTime, 0.0f, 1.0f);
          
			// 머티리얼의 "Percent" 파라미터 업데이트
			CooldownMatDynamic->SetScalarParameterValue(FName("Percent"), Percent);
		}
		
		// 3. 종료 체크
		if (CurrentCooldownTime <= 0.0f)
		{
			bIsCoolingDown = false;
			CurrentCooldownTime = 0.0f;
			
			// UI 원상복구
			if (Img_Dim) Img_Dim->SetVisibility(ESlateVisibility::Hidden);
			if (Txt_Time) Txt_Time->SetVisibility(ESlateVisibility::Hidden);
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
	if (Img_Dim) 
	{
		Img_Dim->SetVisibility(ESlateVisibility::Visible);
        
		// 쿨타임 시작 시 꽉 찬 상태(1.0)로 초기화
		if (CooldownMatDynamic)
		{
			CooldownMatDynamic->SetScalarParameterValue(FName("Percent"), 1.0f);
		}
	}
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
		
		if (Txt_Locked)
		{
			Txt_Locked->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			Txt_Locked->SetText(FText::FromString(TEXT("LOCKED")));
		}
	}
	else
	{
		// 해금됨
		Img_Icon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
		
		// Locked 텍스트 숨기기
		if (Txt_Locked)
		{
			Txt_Locked->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void USkillSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 1. Img_Dim에 설정된 머티리얼을 동적(Dynamic)으로 바꿔서 저장
	if (Img_Dim)
	{
		// 기존에 에디터에서 넣은 M_RadialCooldown을 가져와서 동적 인스턴스로 만듦
		CooldownMatDynamic = Img_Dim->GetDynamicMaterial();
	}
}