#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GQTEWidget.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API UGQTEWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    class UImage* GKeyImage;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* MashText;

    UPROPERTY(EditAnywhere, Category = "QTE|Resources")
    class UTexture2D* Texture_G_Bright;

    UPROPERTY(EditAnywhere, Category = "QTE|Resources")
    class UTexture2D* Texture_G_Dark;

private:
    FTimerHandle FlashTimerHandle;
    bool bIsBright = true;

    void ToggleImage();
};