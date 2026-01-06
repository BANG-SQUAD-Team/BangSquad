// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ServerRow.generated.h"

class UTextBlock;
class UButton;

UCLASS()
class PROJECT_BANG_SQUAD_API UServerRow : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	UTextBlock* ServerName;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* HostUser;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ConnectionFraction;

	UPROPERTY(meta = (BindWidget))
	UButton* RowButton;

	void SetUp(class UMainMenu* InParent, uint32 InIndex);

	UPROPERTY(BlueprintReadOnly)
	bool bSelected = false;

	// ✨ [추가] 색깔 업데이트 함수
	void UpdateColor();

private:
	UFUNCTION()
	void OnClicked();

	UPROPERTY()
	class UMainMenu* Parent;

	uint32 SelfIndex;
};
