// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RespawnWidget.generated.h"

/**
 * 
 */
UCLASS()
class PROJECT_BANG_SQUAD_API URespawnWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void StartCountdown(float Duration);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* CountdownText;

	float LeftTime = 0.f;
	bool bIsCounting = false;
};
