// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/UI/Menu/ServerRow.h"
#include "Components/Button.h"
#include "MainMenu.h"

void UServerRow::SetUp(UMainMenu* InParent, uint32 InIndex)
{
	Parent = InParent;
	SelfIndex = InIndex;
	RowButton->OnClicked.AddDynamic(this, &UServerRow::OnClicked);

	// 처음에 색깔 초기화 (안 하면 랜덤으로 색이 칠해질 수 있음)
	UpdateColor();
}

void UServerRow::OnClicked()
{
	Parent->SetSelectedIndex(SelfIndex);
}

// ✨ [추가] 색깔 바꾸는 함수 구현
void UServerRow::UpdateColor()
{
	if (!RowButton) return;

	// 선택되면 하늘색, 아니면 투명(또는 연한 회색)
	FLinearColor SelectedColor = FLinearColor(0.0f, 0.5f, 1.0f, 1.0f); // 진한 하늘색
	FLinearColor NormalColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.0f);   // 투명 (기본 배경색 보임)

	// bSelected 값에 따라 색 변경
	if (bSelected)
	{
		RowButton->SetBackgroundColor(SelectedColor);
	}
	else
	{
		RowButton->SetBackgroundColor(NormalColor);
	}
}