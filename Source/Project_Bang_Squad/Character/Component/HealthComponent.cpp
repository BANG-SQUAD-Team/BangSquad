#include "HealthComponent.h"

#include "InterchangeResult.h"

UHealthComponent::UHealthComponent()
{
	// 체력 컴포넌트는 매 프레임 업데이트할 필요가 없으므로 끕니다. (최적화)
	PrimaryComponentTick.bCanEverTick = false;

	// 기본값 설정
	MaxHealth = 100.f;
	CurrentHealth = MaxHealth;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	
	// 1. 게임 시작 시 현재 체력을 최대 체력으로 초기화
	CurrentHealth = MaxHealth;
	
	// 2. 주인(Owner) 액터 가져옴
	AActor* Owner = GetOwner();
	if (Owner)
	{
		// 3. 주인이 데미지를 입으면 내 함수 (DamageTaken)가 실행되도록 연결함
		Owner->OnTakeAnyDamage.AddDynamic(this, &UHealthComponent::DamageTaken);
	}
}

void UHealthComponent::SetMaxHealth(float NewMaxHealth)
{
	MaxHealth = NewMaxHealth;
	CurrentHealth = MaxHealth;
}

void UHealthComponent::DamageTaken(AActor* DamagedActor, float Damage, const class UDamageType* DamageType,
		AController* Instigator, AActor* DamageCauser)
{
	// 데미지가 0이하거나 이미 죽은 상태면 무시
	if (Damage <= 0.0f || CurrentHealth <= 0.0f)
	{
		return;
	}
	
	// 1. 체력 감소 ( 0 ~ MaxHealth 사이 값 유지)
	CurrentHealth = FMath::Clamp(CurrentHealth - Damage, 0.0f, MaxHealth);
	
	UE_LOG(LogTemp,Warning, TEXT("Health Changed: %.1f / %.1f"), CurrentHealth, MaxHealth);
	
	// 2. 체력 변했다고 알려주기용
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	
	// 3. 체력 0 되면 사망처리
	if (CurrentHealth <= 0.0f)
	{
		UE_LOG(LogTemp,Warning, TEXT("Character Dead!"));
		OnDead.Broadcast();
		
		// 추후 이곳에 사망 애니메이션 재생이나 액터 파괴 로직 연결
	}
}