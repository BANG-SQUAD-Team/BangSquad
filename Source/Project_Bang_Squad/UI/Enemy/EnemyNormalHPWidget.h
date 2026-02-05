// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnemyNormalHPWidget.generated.h"

class UProgressBar;
UCLASS()
class PROJECT_BANG_SQUAD_API UEnemyNormalHPWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// 캐릭터 쪽에서 이 함수를 호출해 체력바를 갱신
	void UpdateHP(float CurrentHP, float MaxHP);
	
protected:
	// 에디터(WBP)에서 만든 ProgressBar의 이름과 똑같아야 연결
	UPROPERTY(meta = (BindWidget))
	UProgressBar* HPProgressBar;
};

