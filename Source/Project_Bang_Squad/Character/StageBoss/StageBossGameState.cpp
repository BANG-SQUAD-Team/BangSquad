// Source/Project_Bang_Squad/Character/StageBoss/StageBossGameState.cpp

#include "StageBossGameState.h"
#include "Net/UnrealNetwork.h"

AStageBossGameState::AStageBossGameState()
{
	BossCurrentHealth = 100.f;
	BossMaxHealth = 100.f;
	bIsQTEActive = false;
	CurrentQTECount = 0;
	TargetQTECount = 50;
	TeamLives = 10; // 기본 목숨
}

void AStageBossGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 변수 복제 등록
	DOREPLIFETIME(AStageBossGameState, BossCurrentHealth);
	DOREPLIFETIME(AStageBossGameState, BossMaxHealth);
	DOREPLIFETIME(AStageBossGameState, bIsQTEActive);
	DOREPLIFETIME(AStageBossGameState, CurrentQTECount);
	DOREPLIFETIME(AStageBossGameState, TargetQTECount);
	DOREPLIFETIME(AStageBossGameState, TeamLives);
}

// [1] 보스 체력 업데이트 (서버 -> 클라이언트 전파)
void AStageBossGameState::UpdateBossHealth(float NewCurrent, float NewMax)
{
	if (HasAuthority())
	{
		BossCurrentHealth = NewCurrent;
		BossMaxHealth = NewMax;
		OnRep_BossHealth(); // 서버(Host)에서도 즉시 반영
	}
}

// [2] QTE 상태 설정 (서버 -> 클라이언트 전파)
void AStageBossGameState::SetQTEStatus(bool bActive, int32 InTarget)
{
	if (HasAuthority())
	{
		bIsQTEActive = bActive;
		TargetQTECount = InTarget;
		CurrentQTECount = 0; // 시작 시 초기화
		OnRep_IsQTEActive();
	}
}

// [3] QTE 카운트 업데이트 (서버 -> 클라이언트 전파)
void AStageBossGameState::UpdateQTECount(int32 NewCount)
{
	if (HasAuthority())
	{
		CurrentQTECount = NewCount;
		OnRep_QTECounts();
	}
}

// [4] 팀 목숨 설정 (서버 -> 클라이언트 전파)
void AStageBossGameState::SetTeamLives(int32 NewLives)
{
	if (HasAuthority())
	{
		TeamLives = NewLives;
		OnRep_TeamLives();
	}
}

// --- [OnRep 함수들: 클라이언트 UI 갱신용] ---

void AStageBossGameState::OnRep_BossHealth()
{
	OnBossHealthChanged.Broadcast(BossCurrentHealth, BossMaxHealth);
}

void AStageBossGameState::OnRep_IsQTEActive()
{
	OnQTEStateChanged.Broadcast(bIsQTEActive);
}

void AStageBossGameState::OnRep_QTECounts()
{
	OnQTECountUpdated.Broadcast(CurrentQTECount, TargetQTECount);
}

void AStageBossGameState::OnRep_TeamLives()
{
	OnTeamLivesChanged.Broadcast(TeamLives);
}