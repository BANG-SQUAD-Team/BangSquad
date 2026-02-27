#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ADQTEWidget.generated.h"

/**
 * UADQTEWidget
 * A/D 연타 기믹의 진행 상태를 프로그레스 바만 우선적으로 표시하는 위젯입니다.
 */
UCLASS()
class PROJECT_BANG_SQUAD_API UADQTEWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "QTE")
	void UpdateProgressBar(int32 Current, int32 Max);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	class UProgressBar* ADQTEProgressBar;

	UPROPERTY(meta = (BindWidget))
	class UImage* ADQTEKeyImage;

	/** [설정] A가 빨간색인 이미지 (RA.png) */
	UPROPERTY(EditAnywhere, Category = "QTE|Resources")
	class UTexture2D* Texture_A;

	/** [설정] D가 빨간색인 이미지 (RD.jpg) */
	UPROPERTY(EditAnywhere, Category = "QTE|Resources")
	class UTexture2D* Texture_D;

private:
	FTimerHandle FlashTimerHandle;
	bool bIsShowingA = true;

	/** 이미지를 교체하는 내부 함수 */
	void ToggleImage();

public:
	void StartImageFlashing();
	void StopImageFlashing();
};