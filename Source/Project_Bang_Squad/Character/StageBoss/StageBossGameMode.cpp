#include "Project_Bang_Squad/Character/StageBoss/StageBossGameMode.h"

#include "StageBossGameState.h"
#include "StageBossPlayerState.h"
#include "Project_Bang_Squad/Character/StageBoss/Stage1Boss.h"
#include "Project_Bang_Squad/Core/TrueDamageType.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Project_Bang_Squad/Game/Stage/StageGameState.h"

AStageBossGameMode::AStageBossGameMode()
{
	// �⺻�� ����
	BaseRespawnTime = 8.f;
}

void AStageBossGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (AStageBossGameState* GS = GetGameState<AStageBossGameState>())
	{
		GS->SetTeamLives(MaxTeamLives);
	}
}

// --- [QTE ����] ---

void AStageBossGameMode::TriggerSpearQTE(AStage1Boss* BossActor)
{
	if (!HasAuthority() || !IsValid(BossActor)) return;

	CurrentBoss = BossActor;
	AccumulatedInputCount = 0;

	// [�ű�] ������ �ʱ�ȭ
	PlayerTapCounts.Empty();

	CurrentBoss->PlayQTEVisuals(QTEDuration);

	GetWorldTimerManager().SetTimer(QTETimerHandle, this, &AStageBossGameMode::OnQTETimeout, QTEDuration, false);
	UE_LOG(LogTemp, Warning, TEXT("[BossGameMode] QTE Started!"));
}
void AStageBossGameMode::ProcessQTEInput(AController* PlayerController)
{
	if (!GetWorldTimerManager().IsTimerActive(QTETimerHandle)) return;

	// 1. ��ü ī��Ʈ ����
	AccumulatedInputCount++;

	// 2. [�ű�] ���� ��� ����
	if (PlayerController)
	{
		if (AStageBossPlayerState* PS = Cast<AStageBossPlayerState>(PlayerController->PlayerState))
		{
			PS->AddQTECount();
		}
		/*if (PlayerTapCounts.Contains(PlayerController))
		{
			PlayerTapCounts[PlayerController]++;
		}
		else
		{
			PlayerTapCounts.Add(PlayerController, 1);
		}*/
	}

	// ��ǥġ �޼�
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

	// --- [�ű�] ��� ������ ���� (TMap -> TArray) ---
	TArray<FPlayerQTEStat> FinalStats;
	
	// �ʿ� ��ϵ� �÷��̾�� �߰�
	for (auto& Pair : PlayerTapCounts)
	{
		FPlayerQTEStat Stat;
		// PlayerState�� ��ȿ�ϸ� �г��� ���, ������ �⺻��
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

	// --- ���� ó�� ---
	if (bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("QTE Success! Stage Clear!"));
		
		// 1. �������� ���� ���� (��� ��Ÿ�� ���)
		if (IsValid(CurrentBoss))
		{
			CurrentBoss->HandleQTEResult(true);
		}

		// 2. Ŭ���� UI ȣ�� (BP���� ����)
		ShowGameClearUI(FinalStats);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("QTE Failed! Retry Required."));

		// 1. �������� ���� ���� (����ų� ������ ����)
		if (IsValid(CurrentBoss))
		{
			CurrentBoss->HandleQTEResult(false);
		}
		
		// 2. ��Ʈ���� UI ȣ�� (BP���� ����)
		ShowRetryUI(FinalStats);
	}
}

// --- [��Ȱ �� ���� �ý���] ---

void AStageBossGameMode::OnPlayerDied(AController* DeadController)
{
	// RequestRespawn���� ���� �Ͽ�ȭ
	RequestRespawn(DeadController);
}

void AStageBossGameMode::RequestRespawn(AController* Controller)
{
	if (!Controller) return;

	if (ABSPlayerState* PS = Controller->GetPlayerState<ABSPlayerState>())
	{
		float ServerTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
		PS->SetRespawnEndTime(ServerTime + BaseRespawnTime);
	}

	FTimerDelegate RespawnDelegate;
	RespawnDelegate.BindUObject(this, &AStageBossGameMode::AttemptRespawn, Controller);

	FTimerHandle RespawnTimerHandle;
	GetWorldTimerManager().SetTimer(RespawnTimerHandle, RespawnDelegate, BaseRespawnTime, false);
	UE_LOG(LogTemp, Warning, TEXT("8초 뒤 부활"));

	
	// �� ��� ���� ����
	//if (MaxTeamLives > 0)
	//{
	//	MaxTeamLives--;
	//	UE_LOG(LogTemp, Warning, TEXT("Team Life Used. Remaining: %d"), MaxTeamLives);
	//
	//	// [�߿�] �θ� Ŭ����(AStageGameMode)�� ������ ����(��ġ ���, ���� ��)�� �״�� ���
	//	//Super::RequestRespawn(Controller);
	//}
	//else
	//{
	//	UE_LOG(LogTemp, Error, TEXT("No Lives Left. Checking Wipe..."));
	//	CheckPartyWipe();
	//}
}

void AStageBossGameMode::CheckPartyWipe()
{
	bool bAnyAlive = false;

	// ������ �ִ� ��� �÷��̾� ��Ʈ�ѷ� �˻�
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			APawn* MyPawn = PC->GetPawn();

			if (MyPawn)
			{
				ABaseCharacter* BaseChar = Cast<ABaseCharacter>(MyPawn);

				if (BaseChar && !BaseChar->IsDead())
				{
					bAnyAlive = true;
					break;
				}
			}
		}
	}

	if (!bAnyAlive)
	{
		EndStage(false); // �й�
	}
}

void AStageBossGameMode::OnBossKilled()
{
	EndStage(true); // �¸�
}

void AStageBossGameMode::DebugStartQTE()
{
	// 월드에서 보스 액터를 찾습니다.
	AActor* FoundActor = UGameplayStatics::GetActorOfClass(GetWorld(), AStage1Boss::StaticClass());
	if (AStage1Boss* Boss = Cast<AStage1Boss>(FoundActor))
	{
		// 기존에 만들어진 QTE 실행 함수 호출
		TriggerSpearQTE(Boss);
	}
}

void AStageBossGameMode::EndStage(bool bIsVictory)
{
	if (bIsVictory)
	{
		UE_LOG(LogTemp, Warning, TEXT("Boss Cleared! Victory!"));
		// �¸� �� �θ��� �������� �̵� ���� Ȱ�� ����
		// ClearStageAndMove(EStageIndex::Stage2); 
		GiveStageClearReward(100);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Mission Failed."));
		// �й� ó�� (���� ���� UI ��)
		FTimerHandle ReturnTimer;
		GetWorldTimerManager().SetTimer(ReturnTimer, this, &AStageBossGameMode::ReturnToStage, 3.0f, false);
	}

}

void AStageBossGameMode::AttemptRespawn(AController* ControllerToRespawn)
{
	if (!ControllerToRespawn) return;
	AStageBossGameState* GS = GetWorld()->GetGameState<AStageBossGameState>();
	if (GS && GS->TeamLives > 0)
	{
		GS->SetTeamLives(GS->TeamLives - 1);
		UE_LOG(LogTemp, Warning, TEXT("부활 성공! 남은 목숨: %d"), GS->TeamLives);

		ExecuteRespawn(ControllerToRespawn);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("부활 실패: 남은 목숨이 없습니다."));
		CheckPartyWipe();
	}
}

void AStageBossGameMode::ReturnToStage()
{
	if (UBSGameInstance* GI = Cast<UBSGameInstance>(GetGameInstance()))
	{
		GI->MoveToStage(GI->GetCurrentStage(), EStageSection::Main);
	}
}

