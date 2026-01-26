// Source/Project_Bang_Squad/Character/StageBoss/StageBossBase.cpp

#include "StageBossBase.h"
#include "Net/UnrealNetwork.h"

AStageBossBase::AStageBossBase()
{
    // 멀티플레이어 설정
    bReplicates = true;
    NetUpdateFrequency = 100.f; // 보스는 움직임과 상태 변화가 중요하므로 빈도를 높게 유지
}

void AStageBossBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // [중복 제거 및 최적화] 모든 복제 변수를 한 곳에서 등록합니다.
    DOREPLIFETIME(AStageBossBase, CurrentPhase);
    DOREPLIFETIME(AStageBossBase, bIsInvincible);
    DOREPLIFETIME(AStageBossBase, bIsActionInProgress);
}

float AStageBossBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    // [권한 분리] 데미지 계산은 반드시 서버(Authority)에서만 수행
    if (!HasAuthority()) return 0.0f;

    // 무적 상태라면 데미지를 계산하지 않고 0을 반환하여 방어
    if (bIsInvincible) return 0.0f;

    return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void AStageBossBase::SetPhase(EBossPhase NewPhase)
{
    if (!HasAuthority()) return;
    if (CurrentPhase == NewPhase) return;

    CurrentPhase = NewPhase;

    // 서버에서의 로직 수행
    OnPhaseChanged(CurrentPhase);

    // 서버는 OnRep이 자동으로 호출되지 않으므로 수동 호출하여 동기화 로직 실행
    OnRep_CurrentPhase();
}

void AStageBossBase::OnRep_CurrentPhase()
{
    // [비주얼 동기화] 클라이언트에서 페이즈 변경에 따른 UI 업데이트나 이펙트 처리를 수행
}

void AStageBossBase::OnPhaseChanged(EBossPhase NewPhase)
{
    // 자식 클래스(Stage1Boss 등)에서 상세 페이즈 전이 로직 구현
}

void AStageBossBase::OnGimmickResolved(int32 GimmickID)
{
    // 자식 클래스에서 기믹 클리어 로직 구현
}

void AStageBossBase::ServerSetActionInProgress_Implementation(bool bInProgress)
{
    // 서버에서만 상태를 안전하게 변경하도록 보장
    if (HasAuthority())
    {
        bIsActionInProgress = bInProgress;

        // 디버그 로그: 상태 변화 추적용
        // UE_LOG(LogTemp, Log, TEXT("Boss Action State Changed: %s"), bInProgress ? TEXT("TRUE") : TEXT("FALSE"));
    }
}