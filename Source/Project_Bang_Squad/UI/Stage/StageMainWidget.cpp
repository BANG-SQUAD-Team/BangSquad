// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/UI/Stage/StageMainWidget.h"

#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"

void UStageMainWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CachedPlayerCount = 0;
}

void UStageMainWidget::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
	Super::NativeTick(MyGeometry, DeltaTime);

	if (AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>())
	{
		bool bNeedUpdate = false;

		//전체 플레이어 수 변경 감지
		if (GS->PlayerArray.Num() != CachedPlayerCount)
		{
			bNeedUpdate = true;
		}
		else
		{
			//1초마다 UI 개수 정합성 체크
			static float TimeAccumulator = 0.f;
			TimeAccumulator += DeltaTime;
			
			if (TimeAccumulator > 1.f)
			{
				TimeAccumulator = 0.f;
				bNeedUpdate = true;
				/*if (PlayerListContainer)
				{
					int32 SelfCount = (GetOwningPlayerState()) ? 1 : 0;
					int32 ExpectedChildCount = GS->PlayerArray.Num() - SelfCount;

					ExpectedChildCount = FMath::Max(ExpectedChildCount, 0);

					if (PlayerListContainer->GetChildrenCount() != ExpectedChildCount)
					{
						bNeedUpdate = true;
					}
				}*/
				/*else if (PlayerListContainer)
				{
					for (int32 i = 0; i < GS->PlayerArray.Num(); ++i)
					{
						UPlayerRow* Row = Cast<UPlayerRow>(PlayerListContainer->GetChildAt(i));

						if (!Row || Row->GetTargetPlayerState() != GS->PlayerArray[i])
						{
							bNeedUpdate = true;
							break;
						}
					}
				}*/
			}
		}

		if (bNeedUpdate)
		{
			UpdatePartyList();
			CachedPlayerCount = GS->PlayerArray.Num();
		}
	}
	
	// 🔥 리스폰 등으로 캐릭터가 바뀌었는지 체크
	if (APlayerController* PC = GetOwningPlayer())
	{
		APawn* CurrentPawn = PC->GetPawn();
		if (CurrentPawn && CurrentPawn != CachedCharacter.Get())
		{
			BindCharacterDelegates(CurrentPawn);
		}
	}
}

void UStageMainWidget::BindCharacterDelegates(APawn* NewPawn)
{
	ABaseCharacter* NewChar = Cast<ABaseCharacter>(NewPawn);
    
	// 기존 연결 해제 (이전 캐릭터가 있다면)
	if (CachedCharacter.IsValid())
	{
		CachedCharacter->OnSkillCooldownChanged.RemoveDynamic(this, &UStageMainWidget::OnSkillCooldown);
	}

	// 새 캐릭터 연결
	if (NewChar)
	{
		CachedCharacter = NewChar;
		NewChar->OnSkillCooldownChanged.AddDynamic(this, &UStageMainWidget::OnSkillCooldown);

		// 캐릭터의 데이터 테이블에서 아이콘을 가져와서 UI에 세팅
        
		if (SkillSlot_1)
		{
			FName RowName = FName("Skill1");
          
			// 1. 아이콘 설정
			UTexture2D* Icon = NewChar->GetSkillIconByRowName(RowName);
			if (Icon) SkillSlot_1->SetIcon(Icon);
          
			// 2.  잠금 상태 확인 및 적용
			// (BaseCharacter에 IsSkillUnlockedByRowName 함수가 있어야 합니다!)
			bool bUnlocked = NewChar->IsSkillUnlockedByRowName(RowName);
          
			// 해금되지 않았으면(!bUnlocked) -> 잠금 상태(true)로 설정
			SkillSlot_1->SetSlotLockedState(!bUnlocked); 
		}

		// [스킬 2] 
		if (SkillSlot_2)
		{
			FName RowName = FName("Skill2");
          
			UTexture2D* Icon = NewChar->GetSkillIconByRowName(RowName);
			if (Icon) SkillSlot_2->SetIcon(Icon);

			// 잠금 상태 적용
			bool bUnlocked = NewChar->IsSkillUnlockedByRowName(RowName);
			SkillSlot_2->SetSlotLockedState(!bUnlocked);
		}

		// [직업 스킬] (Space/Shift)
		if (SkillSlot_Job)
		{
			FName RowName = FName("JobAbility");
          
			UTexture2D* Icon = NewChar->GetSkillIconByRowName(RowName);
			if (Icon) SkillSlot_Job->SetIcon(Icon);

			//  잠금 상태 적용
			bool bUnlocked = NewChar->IsSkillUnlockedByRowName(RowName);
			SkillSlot_Job->SetSlotLockedState(!bUnlocked);
		}
		
		// 스킬 1 이름 
		if (Skill1Text)
		{
			FText Name = NewChar->GetSkillNameTextByRowName(FName("Skill1"));
			Skill1Text->SetText(Name);
		}

		// 스킬 2 이름 
		if (Skill2Text)
		{
			FText Name = NewChar->GetSkillNameTextByRowName(FName("Skill2"));
			Skill2Text->SetText(Name);
		}

		// 직업 스킬 이름 
		if (JobAbilityText)
		{
			FText Name = NewChar->GetSkillNameTextByRowName(FName("JobAbility"));
			JobAbilityText->SetText(Name);
		}
	}
}

void UStageMainWidget::OnSkillCooldown(int32 SkillIndex, float CooldownTime)
{
	// 캐릭터 코드에서 TriggerSkillCooldown(인덱스, 시간)을 호출하면 여기가 실행됨
    
	switch (SkillIndex)
	{
	case 1: // Skill 1
		if (SkillSlot_1) SkillSlot_1->StartCooldown(CooldownTime);
		break;
        
	case 2: // Skill 2 
		if (SkillSlot_2) SkillSlot_2->StartCooldown(CooldownTime);
		break;
        
	case 3: // Job Ability 
		if (SkillSlot_Job) SkillSlot_Job->StartCooldown(CooldownTime);
		break;
	}
}

void UStageMainWidget::UpdatePartyList()
{
	if (!PlayerListContainer || !PlayerRowClass) return;

	AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>();
	if (!GS) return;

	APlayerState* MyPS = GetOwningPlayerState();
	int32 CurrentChildIndex = 0;

	// 4. 플레이어 목록 순회하며 생성
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS) continue;
		if (PS == MyPS)
		{
			//내 정보 업데이트
			if (MyInfoRow)
			{
				MyInfoRow->SetTargetPlayerState(PS);
				MyInfoRow->SetWidgetMode(ERowMode::Stage);
				//MyInfoRow->UpdateStageInfo();
				MyInfoRow->SetVisibility(ESlateVisibility::Visible);
			}
			continue;
		}
		//팀원 위젯 처리
		UPlayerRow* Row = nullptr;
		
		if (CurrentChildIndex < PlayerListContainer->GetChildrenCount())
		{
			Row = Cast<UPlayerRow>(PlayerListContainer->GetChildAt(CurrentChildIndex));
		}
		if (!Row)
		{
			Row = CreateWidget<UPlayerRow>(this, PlayerRowClass);
			if (Row)
			{
				PlayerListContainer->AddChild(Row);
			}
		}

		if (Row)
		{
			//누구 정보인지 타겟 설정
			Row->SetTargetPlayerState(PS);
			//모드 설정 (Stage 모드로 변경 -> HP바 켜짐 등)
			Row->SetWidgetMode(ERowMode::Stage);
			//님이 만든 그 정보 갱신 함수 호출 (부활시간, 프로필, HP바 등 한방에 세팅)
			//Row->UpdateStageInfo();
			CurrentChildIndex++;

		}
	}
	while (PlayerListContainer->GetChildrenCount() > CurrentChildIndex)
	{
		PlayerListContainer->RemoveChildAt(PlayerListContainer->GetChildrenCount() - 1);
	}
}
