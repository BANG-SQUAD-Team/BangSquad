// Source/Project_Bang_Squad/Game/StageBoss/StageBossGameMode.cpp
#include "StageBossGameMode.h"
#include "StageBossPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h" 

AStageBossGameMode::AStageBossGameMode()
{
	// 1. 초기값 설정
	CurrentTeamLives = MaxTeamLives;

	// 2. 기본 컨트롤러 클래스 지정 (중요!)
	// BP에서 덮어쓸 수도 있지만, C++ 차원에서도 연결해둡니다.
	PlayerControllerClass = AStageBossPlayerController::StaticClass();
}

void AStageBossGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 시작 시 모든 클라이언트에게 초기 목숨(10개) 동기화가 필요하다면 여기서 처리 가능
	// (플레이어 접속 시점인 PostLogin에서 처리하는 것이 더 정확함)
}

void AStageBossGameMode::OnBossKilled()
{
	// 보스 사망 -> 승리 처리
	UE_LOG(LogTemp, Warning, TEXT("StageBossGameMode: Boss Killed! Victory!"));
	EndStage(true);
}

void AStageBossGameMode::OnPlayerDied(AController* DeadController)
{
	// 1. 이미 전멸 상태거나 목숨이 0개라면? -> 전멸 체크로 바로 이동
	if (CurrentTeamLives <= 0)
	{
		CheckPartyWipe();
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Player Died: %s. Respawn in %.1f sec"), *DeadController->GetName(), RespawnDelay);

	// 2. 부활 타이머 시작 (8초 뒤 AttemptRespawn 호출)
	FTimerHandle RespawnTimer;
	FTimerDelegate RespawnDelegate;
	RespawnDelegate.BindUObject(this, &AStageBossGameMode::AttemptRespawn, DeadController);

	GetWorldTimerManager().SetTimer(RespawnTimer, RespawnDelegate, RespawnDelay, false);

	// 3. 해당 플레이어에게 "부활 대기 중..." 타이머 UI 표시 알림
	if (AStageBossPlayerController* PC = Cast<AStageBossPlayerController>(DeadController))
	{
		PC->Client_ShowRespawnTimer(RespawnDelay);
	}
}

void AStageBossGameMode::AttemptRespawn(AController* ControllerToRespawn)
{
	// 1. 목숨 확인
	if (CurrentTeamLives > 0)
	{
		// 2. 플레이어 부활 (PlayerStart 위치 중 하나에서 스폰)
		RestartPlayer(ControllerToRespawn);

		// 3. 목숨 차감
		CurrentTeamLives--;

		// 4. 모든 플레이어에게 목숨 갱신 알림 (UI 업데이트)
		for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (AStageBossPlayerController* PC = Cast<AStageBossPlayerController>(It->Get()))
			{
				PC->Client_UpdateTeamLives(CurrentTeamLives);
			}
		}

		UE_LOG(LogTemp, Log, TEXT("Player Respawned. Lives Left: %d"), CurrentTeamLives);
	}
	else
	{
		// 목숨이 다 떨어짐 -> 전멸 체크
		CheckPartyWipe();
	}
}

void AStageBossGameMode::CheckPartyWipe()
{
	// 목숨이 남아있으면 전멸 아님
	if (CurrentTeamLives > 0) return;

	// 살아있는(Pawn이 존재하는) 생존자 확인
	bool bAnySurvivor = false;
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			// Pawn이 있고 숨겨지지 않았으면 생존으로 간주
			if (PC->GetPawn() && !PC->GetPawn()->IsHidden())
			{
				bAnySurvivor = true;
				break;
			}
		}
	}

	// 생존자 0명 + 목숨 0개 = 완전 패배
	if (!bAnySurvivor)
	{
		UE_LOG(LogTemp, Error, TEXT("ALL PLAYERS DEAD & NO LIVES LEFT -> RESET STAGE"));
		// 패배 처리 (재시작)
		RestartStage();
	}
}

void AStageBossGameMode::EndStage(bool bIsVictory)
{
	// 모든 플레이어에게 결과 통보
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AStageBossPlayerController* PC = Cast<AStageBossPlayerController>(It->Get()))
		{
			PC->Client_OnStageEnded(bIsVictory);
		}
	}
}

void AStageBossGameMode::RestartStage()
{
	// 레벨 재시작 (ServerTravel ?Restart)
	GetWorld()->ServerTravel("?Restart", false);
}