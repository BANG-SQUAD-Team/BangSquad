// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/UI/Enemy/GQTEWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UGQTEWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 0.15초마다 이미지를 교체해서 반짝이는 효과 부여
    GetWorld()->GetTimerManager().SetTimer(FlashTimerHandle, this, &UGQTEWidget::ToggleImage, 0.15f, true);
}

void UGQTEWidget::ToggleImage()
{
    if (!GKeyImage || !Texture_G_Bright || !Texture_G_Dark) return;

    // 밝은 이미지와 어두운 이미지를 번갈아 세팅
    GKeyImage->SetBrushFromTexture(bIsBright ? Texture_G_Bright : Texture_G_Dark);

    bIsBright = !bIsBright;
}