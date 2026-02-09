// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "SkillSlotWidget.generated.h"


UCLASS()
class PROJECT_BANG_SQUAD_API USkillSlotWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	virtual void NativeTick(const FGeometry& MyGeoMetry, float DeltaTime) override;
	
	// 외부 (StageMainWidget)에서 쿨타임 시작하라고 부를 함수
	UFUNCTION(BlueprintCallable)
	void StartCooldown(float Duration);
	
	// 스킬 아이콘 변경 함수
	void SetIcon(UTexture2D* NewIcon);
protected:
	UPROPERTY(meta = (BindWidget))
	UImage* Img_Icon; // 스킬 아이콘
	
	UPROPERTY(meta = (BindWidget))
	UImage* Img_Dim; // 쿨타임 중 어둡게 가리는 이미지
	
	UPROPERTY(meta = (BindWidget))
	UTextBlock* Txt_Time; // 남은 시간 텍스트
	
	UPROPERTY(meta = (BindWidgetOptional))
	UProgressBar* PB_Cooldown; // 쿨타임 게이지
	
private:
	bool bIsCoolingDown = false;
	float CurrentCooldownTime = 0.0f;
	float MaxCooldownTime = 1.0f;
};
