#include "StageBossBase.h"
#include "Net/UnrealNetwork.h"

AStageBossBase::AStageBossBase()
{
    bReplicates = true;
    NetUpdateFrequency = 100.f; // 보스는 중요하므로 갱신 빈도 높임
}

void AStageBossBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AStageBossBase, CurrentPhase);
    DOREPLIFETIME(AStageBossBase, bIsInvincible);
}

float AStageBossBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (!HasAuthority()) return 0.0f;

    // 무적이면 데미지 완전 무시
    if (bIsInvincible) return 0.0f;

    return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void AStageBossBase::SetPhase(EBossPhase NewPhase)
{
    if (!HasAuthority()) return;
    if (CurrentPhase == NewPhase) return;

    CurrentPhase = NewPhase;
    OnPhaseChanged(CurrentPhase);
    OnRep_CurrentPhase(); // 서버에서도 호출
}

void AStageBossBase::OnRep_CurrentPhase()
{
    // 클라이언트: UI 갱신, BGM 변경 등은 여기서 BP로 처리하면 됨
}

void AStageBossBase::OnPhaseChanged(EBossPhase NewPhase)
{
    // 자식 클래스에서 구현
}

void AStageBossBase::OnGimmickResolved(int32 GimmickID)
{
    // 자식 클래스에서 구현
}