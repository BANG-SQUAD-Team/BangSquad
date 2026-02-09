// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Project_Bang_Squad/UI/Stage/PlayerRow.h"
#include "Project_Bang_Squad/UI/Stage/SkillSlotWidget.h" 
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "StageMainWidget.generated.h"

/**
 * 
 */
UCLASS()
class PROJECT_BANG_SQUAD_API UStageMainWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;
	
public:
	UPROPERTY(meta = (BindWidget))
	USkillSlotWidget* SkillSlot_1; // 스킬 1

	UPROPERTY(meta = (BindWidget))
	USkillSlotWidget* SkillSlot_2; // 스킬 2

	UPROPERTY(meta = (BindWidget))
	USkillSlotWidget* SkillSlot_Job; // 직업 능력
	
	UPROPERTY(meta = (BindWidget))
	UPlayerRow* MyInfoRow;
	
	UPROPERTY(meta = (BindWidget))
	UVerticalBox* PlayerListContainer;

	UPROPERTY(EditDefaultsOnly, Category = "BS|UI")
	TSubclassOf<UPlayerRow> PlayerRowClass;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* Skill1Text;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* Skill2Text;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* JobAbilityText;
	
	void UpdatePartyList();
	
	// 델리게이트 바인딩 함수
	void BindCharacterDelegates(APawn* NewPawn);
    
	//  델리게이트 수신 함수
	UFUNCTION()
	void OnSkillCooldown(int32 SkillIndex, float CooldownTime);

private:
	int32 CachedPlayerCount = 0;
	
	// 현재 연결된 캐릭터 캐싱 (중복 바인딩 방지)
	TWeakObjectPtr<ABaseCharacter> CachedCharacter;
};
