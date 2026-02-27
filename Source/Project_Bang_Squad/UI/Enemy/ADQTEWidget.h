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
	UPROPERTY(meta = (BindWidget))
	class UProgressBar* ADQTEProgressBar;
};