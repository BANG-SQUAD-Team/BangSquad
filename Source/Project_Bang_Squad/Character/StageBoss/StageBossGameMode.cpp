#include "Project_Bang_Squad/Character/StageBoss/StageBossGameMode.h"
#include "Project_Bang_Squad/Character/StageBoss/Stage1Boss.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

AStageBossGameMode::AStageBossGameMode()
{
	// 기본값 설정
}

void AStageBossGameMode::BeginPlay()
{
	Super::BeginPlay();
}

// --- [QTE 로직] ---

void AStageBossGameMode::TriggerSpearQTE(AStage1Boss* BossActor)
{
	if (!HasAuthority() || !IsValid(BossActor)) return;

	CurrentBoss = BossActor;
	AccumulatedInputCount = 0;

	// [신규] 점수판 초기화
	PlayerTapCounts.Empty();

	CurrentBoss->PlayQTEVisuals(QTEDuration);

	GetWorldTimerManager().SetTimer(QTETimerHandle, this, &AStageBossGameMode::OnQTETimeout, QTEDuration, false);
	UE_LOG(LogTemp, Warning, TEXT("[BossGameMode] QTE Started!"));
}
void AStageBossGameMode::ProcessQTEInput(AController* PlayerController)
{
	if (!GetWorldTimerManager().IsTimerActive(QTETimerHandle)) return;

	// 1. 전체 카운트 증가
	AccumulatedInputCount++;

	// 2. [신규] 개인 기록 갱신
	if (PlayerController)
	{
		if (PlayerTapCounts.Contains(PlayerController))
		{
			PlayerTapCounts[PlayerController]++;
		}
		else
		{
			PlayerTapCounts.Add(PlayerController, 1);
		}
	}

	// 목표치 달성
	if (AccumulatedInputCount >= GoalQTECount)
	{
		EndSpearQTE(true);
	}
}

void AStageBossGameMode::OnQTETimeout()
{
	EndSpearQTE(false);
}

void AStageBossGameMode::EndSpearQTE(bool bSuccess)
{
	GetWorldTimerManager().ClearTimer(QTETimerHandle);

	// --- [신규] 통계 데이터 가공 (TMap -> TArray) ---
	TArray<FPlayerQTEStat> FinalStats;
	
	// 맵에 기록된 플레이어들 추가
	for (auto& Pair : PlayerTapCounts)
	{
		FPlayerQTEStat Stat;
		// PlayerState가 유효하면 닉네임 사용, 없으면 기본값
		if (Pair.Key && Pair.Key->PlayerState)
		{
			Stat.PlayerName = Pair.Key->PlayerState->GetPlayerName();
		}
		else
		{
			Stat.PlayerName = TEXT("Unknown Player");
		}
		Stat.TapCount = Pair.Value;
		FinalStats.Add(Stat);
	}

	// --- 승패 처리 ---
	if (bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("QTE Success! Stage Clear!"));
		
		// 1. 보스에게 성공 전달 (사망 몽타주 재생)
		if (IsValid(CurrentBoss))
		{
			CurrentBoss->HandleQTEResult(true);
		}

		// 2. 클리어 UI 호출 (BP에서 구현)
		ShowGameClearUI(FinalStats);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("QTE Failed! Retry Required."));

		// 1. 보스에게 실패 전달 (비웃거나 가만히 있음)
		if (IsValid(CurrentBoss))
		{
			CurrentBoss->HandleQTEResult(false);
		}
		
		// 2. 리트라이 UI 호출 (BP에서 구현)
		ShowRetryUI(FinalStats);
	}
}

// --- [부활 및 전멸 시스템] ---

void AStageBossGameMode::OnPlayerDied(AController* DeadController)
{
	// RequestRespawn으로 로직 일원화
	RequestRespawn(DeadController);
}

void AStageBossGameMode::RequestRespawn(AController* Controller)
{
	if (!Controller) return;

	// 팀 목숨 차감 로직
	if (MaxTeamLives > 0)
	{
		MaxTeamLives--;
		UE_LOG(LogTemp, Warning, TEXT("Team Life Used. Remaining: %d"), MaxTeamLives);

		// [중요] 부모 클래스(AStageGameMode)의 리스폰 로직(위치 계산, 스폰 등)을 그대로 사용
		Super::RequestRespawn(Controller);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("No Lives Left. Checking Wipe..."));
		CheckPartyWipe();
	}
}

void AStageBossGameMode::CheckPartyWipe()
{
	bool bAnyAlive = false;

	// 서버에 있는 모든 플레이어 컨트롤러 검사
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			// 폰이 살아있다면 전멸 아님
			if (PC->GetPawn())
			{
				bAnyAlive = true;
				break;
			}
		}
	}

	if (!bAnyAlive)
	{
		EndStage(false); // 패배
	}
}

void AStageBossGameMode::OnBossKilled()
{
	EndStage(true); // 승리
}

void AStageBossGameMode::EndStage(bool bIsVictory)
{
	if (bIsVictory)
	{
		UE_LOG(LogTemp, Warning, TEXT("Boss Cleared! Victory!"));
		// 승리 시 부모의 스테이지 이동 로직 활용 가능
		// ClearStageAndMove(EStageIndex::Stage2); 
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Mission Failed."));
		// 패배 처리 (게임 오버 UI 등)
	}
}