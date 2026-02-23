#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DamageTextActor.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API ADamageTextActor : public AActor
{
	GENERATED_BODY()
    
public:    
	ADamageTextActor();
	virtual void Tick(float DeltaTime) override;

	// UI를 화면에 띄워줄 위젯 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	class UWidgetComponent* DamageWidgetComp;

	// 몬스터가 맞았을 때 데미지 숫자를 넘겨받을 함수
	UFUNCTION(BlueprintCallable, Category = "UI")
	void InitializeDamage(float DamageAmount);

protected:
	virtual void BeginPlay() override;
};