#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

// 1. 델리게이트 선언 (체력이 변했을 때 알림을 보낼 방송국 정의)
// 파라미터: 현재 체력, 최대 체력
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, CurrentHealth, float, MaxHealth);

// 사망했을 때 알릴 방송국
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDead);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROJECT_BANG_SQUAD_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public: 
	UHealthComponent();
	
protected:
	virtual void BeginPlay() override;
	
public:
	// 최대 체력 (에디터에서 수정 가능함)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float MaxHealth = 100;
	
	// 현재 체력 (보기 전용)
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Health")
	float CurrentHealth;
	
	// 2. 실제 방송국 변수 (블루프린트에서 바인딩 가능)
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnHealthChanged OnHealthChanged;
	
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnDead OnDead;
	
	/** 외부에서 최대 체력을 변경할 때 사용 (직업별 체력 설정용) */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void SetMaxHealth(float NewMaxHealth);
	
	/** 현재 체력 가져오기 */
	UFUNCTION(BlueprintCallable, Category = "Health")
	float GetHealth() const {return CurrentHealth;}
	
protected:
	/**
	 *주인(Owner)이 데미지 입었을 때 자동으로 호출될 함수
	 *언리얼 엔진의 OnTakeAnyDamage 델리게이트 시그니처와 정확히 일치해야함.
	 *(const UDamageType* 부분이 중요)
	 */
	UFUNCTION()
	void DamageTaken(AActor* DamagedActor, float Damage, const class UDamageType* DamageType,
		AController* Instigator, AActor* DamageCauser);
};