#include "StageBossGameMode.h"
#include "StageBossGameState.h"
#include "Stage1Boss.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

AStageBossGameMode::AStageBossGameMode()
{
	GameStateClass = AStageBossGameState::StaticClass();
	MaxTeamLives = 10;
}

void AStageBossGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 게임 시작 시 GameState에 초기 목숨 설정
	if (AStageBossGameState* GS = GetGameState<AStageBossGameState>())
	{
		GS->SetTeamLives(MaxTeamLives);
	}
}

// ============================================================================
// [1] QTE Logic
// ============================================================================
void AStageBossGameMode::TriggerSpearQTE(AStage1Boss* BossActor)
{
	CurrentBoss = BossActor;
	AccumulatedInputCount = 0;

	if (AStageBossGameState* GS = GetGameState<AStageBossGameState>())
		GS->SetQTEStatus(true, GoalQTECount);

	if (CurrentBoss) CurrentBoss->PlayQTEVisuals(QTEDuration);

	GetWorldTimerManager().SetTimer(QTETimerHandle, this, &AStageBossGameMode::OnQTETimeout, QTEDuration, false);
}

void AStageBossGameMode::ProcessQTEInput(AController* PlayerController)
{
	if (!GetWorldTimerManager().IsTimerActive(QTETimerHandle)) return;

	AccumulatedInputCount++;
	if (AStageBossGameState* GS = GetGameState<AStageBossGameState>())
		GS->UpdateQTECount(AccumulatedInputCount);

	if (AccumulatedInputCount >= GoalQTECount) EndSpearQTE(true);
}

void AStageBossGameMode::OnQTETimeout() { EndSpearQTE(false); }

void AStageBossGameMode::EndSpearQTE(bool bSuccess)
{
	GetWorldTimerManager().ClearTimer(QTETimerHandle);
	if (AStageBossGameState* GS = GetGameState<AStageBossGameState>())
		GS->SetQTEStatus(false, GoalQTECount);

	if (CurrentBoss) CurrentBoss->HandleQTEResult(bSuccess);
}

// ============================================================================
// [2] Death & Respawn Logic
// ============================================================================

void AStageBossGameMode::OnPlayerDied(AController* DeadController)
{
	if (!DeadController) return;

	// 현재 목숨 확인 (GameState에서 가져옴)
	int32 CurrentLives = 0;
	if (AStageBossGameState* GS = GetGameState<AStageBossGameState>())
	{
		CurrentLives = GS->TeamLives;
	}

	// 목숨이 없으면 전멸 체크 후 리턴
	if (CurrentLives <= 0)
	{
		CheckPartyWipe();
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Player Died. Respawn in %.1f sec"), RespawnDelay);

	// 타이머 설정 (AttemptRespawn 호출)
	FTimerHandle RespawnTimer;
	FTimerDelegate RespawnDelegate;
	RespawnDelegate.BindUObject(this, &AStageBossGameMode::AttemptRespawn, DeadController);
	GetWorldTimerManager().SetTimer(RespawnTimer, RespawnDelegate, RespawnDelay, false);
}

void AStageBossGameMode::AttemptRespawn(AController* ControllerToRespawn)
{
	AStageBossGameState* GS = GetGameState<AStageBossGameState>();
	if (!GS) return;

	if (GS->TeamLives > 0)
	{
		// 1. 목숨 차감 (GameState 업데이트)
		GS->SetTeamLives(GS->TeamLives - 1);

		// 2. 부모 클래스의 스폰 함수 혹은 기본 RestartPlayer 사용
		// AStageGameMode::SpawnPlayerCharacter를 사용하는 것이 직업 유지에 유리함
		// ExecuteRespawn(ControllerToRespawn); // 부모 클래스 함수 활용 권장
		RestartPlayer(ControllerToRespawn); // 혹은 기본 부활

		UE_LOG(LogTemp, Log, TEXT("Player Respawned. Lives Left: %d"), GS->TeamLives);
	}
	else
	{
		CheckPartyWipe();
	}
}

void AStageBossGameMode::CheckPartyWipe()
{
	AStageBossGameState* GS = GetGameState<AStageBossGameState>();
	if (GS && GS->TeamLives > 0) return; // 목숨 남았으면 전멸 아님

	// 살아있는 플레이어 Pawn 확인
	bool bAnySurvivor = false;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (PC->GetPawn() && !PC->GetPawn()->IsHidden())
			{
				bAnySurvivor = true;
				break;
			}
		}
	}

	if (!bAnySurvivor)
	{
		UE_LOG(LogTemp, Error, TEXT(">>> GAME OVER: PARTY WIPE <<<"));
		EndStage(false);
	}
}

void AStageBossGameMode::OnBossKilled()
{
	UE_LOG(LogTemp, Warning, TEXT(">>> VICTORY: BOSS KILLED <<<"));
	EndStage(true);
}

void AStageBossGameMode::EndStage(bool bIsVictory)
{
	// TODO: 결과창 UI 띄우기, 로비로 이동 등
	if (!bIsVictory)
	{
		// 5초 뒤 재시작
		FTimerHandle RestartTimer;
		GetWorldTimerManager().SetTimer(RestartTimer, this, &AStageBossGameMode::RestartStage, 5.0f, false);
	}
}

void AStageBossGameMode::RestartStage()
{
	GetWorld()->ServerTravel("?Restart", false);
}