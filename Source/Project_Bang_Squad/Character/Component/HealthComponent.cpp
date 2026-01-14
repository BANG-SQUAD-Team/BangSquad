#include "Project_Bang_Squad/Character/Component/HealthComponent.h"
#include "Net/UnrealNetwork.h" // [필수] 리플리케이션 헤더

UHealthComponent::UHealthComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    // [필수] 컴포넌트 자체 복제 활성화
    SetIsReplicatedByDefault(true);

    MaxHealth = 100.f;
    CurrentHealth = MaxHealth;
}

// [필수] 동기화할 변수 등록
void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // CurrentHealth 변수를 동기화하겠다고 등록
    DOREPLIFETIME(UHealthComponent, CurrentHealth);
}

void UHealthComponent::BeginPlay()
{
    Super::BeginPlay();
    
    // 서버(권한이 있는 쪽)에서만 체력을 초기화
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        CurrentHealth = MaxHealth;
    }

    // [삭제됨] Owner->OnTakeAnyDamage.AddDynamic(...) 
    // 이유: BaseCharacter::TakeDamage 안에서 ApplyDamage를 직접 호출하므로, 
    // 여기서 또 바인딩하면 데미지가 2배로 들어갈 위험이 있음.
}

void UHealthComponent::SetMaxHealth(float NewMaxHealth)
{
    // 서버에서만 설정 가능하도록 제한하는 것이 좋음
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        MaxHealth = NewMaxHealth;
        CurrentHealth = MaxHealth;
        
        // 변경된 즉시 서버에서도 델리게이트 호출
        OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
    }
}

bool UHealthComponent::IsDead() const
{
    return CurrentHealth <= 0.0f;
}

// 기존 DamageTaken 역할을 하는 함수 (서버에서만 호출됨)
void UHealthComponent::ApplyDamage(float DamageAmount)
{
    // 이미 죽었거나 데미지가 없으면 무시
    if (DamageAmount <= 0.0f || IsDead())
    {
       return;
    }
    
    // 1. 체력 감소 (0 ~ MaxHealth 사이 값 유지)
    CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, MaxHealth);
    
    // 2. 서버 쪽 델리게이트 방송 (서버 로직/AI 반응용)
    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
    
    // 3. 사망 처리
    if (IsDead())
    {
       UE_LOG(LogTemp, Warning, TEXT("Character Dead! (Server)"));
       OnDead.Broadcast();
    }
}

// 서버에서 CurrentHealth가 변하면 클라이언트에서 이 함수가 자동 실행됨
void UHealthComponent::OnRep_CurrentHealth()
{
    // 1. 클라이언트 UI 갱신을 위해 델리게이트 방송
    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

    // 2. 사망 처리 (클라이언트에서 래그돌이나 이펙트 재생용)
    if (IsDead())
    {
        UE_LOG(LogTemp, Warning, TEXT("Character Dead! (Client)"));
        OnDead.Broadcast();
    }
}