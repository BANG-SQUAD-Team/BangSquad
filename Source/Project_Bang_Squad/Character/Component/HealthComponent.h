#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

// 1. 델리게이트 선언 (기존 유지)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDead);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROJECT_BANG_SQUAD_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public: 
	UHealthComponent();

	// [필수] 변수 동기화 목록 등록
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    
protected:
	virtual void BeginPlay() override;
    
public:
	// 최대 체력
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float MaxHealth = 100;
    
	// [중요] 현재 체력 (ReplicatedUsing 추가 -> 서버에서 바뀌면 클라이언트의 OnRep_CurrentHealth 실행)
	UPROPERTY(ReplicatedUsing = OnRep_CurrentHealth, VisibleAnywhere, BlueprintReadWrite, Category = "Health")
	float CurrentHealth;

	// 클라이언트 동기화 함수
	UFUNCTION()
	void OnRep_CurrentHealth();
    
	// 실제 방송국 변수 (기존 유지)
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnHealthChanged OnHealthChanged;
    
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnDead OnDead;
    
	UFUNCTION(BlueprintCallable, Category = "Health")
	void SetMaxHealth(float NewMaxHealth);
    
	UFUNCTION(BlueprintCallable, Category = "Health")
	float GetHealth() const { return CurrentHealth; }

	// [추가] 죽었는지 확인 (BaseCharacter에서 사용)
	UFUNCTION(BlueprintCallable, Category = "Health")
	bool IsDead() const;

	// [변경] BaseCharacter의 TakeDamage에서 직접 호출할 함수
	// (기존 DamageTaken 대신 이 함수가 데미지 처리를 담당)
	void ApplyDamage(float DamageAmount);
};