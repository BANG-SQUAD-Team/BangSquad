#include "BSGameMode.h"
#include "Project_Bang_Squad/Game/Base/BSPlayerState.h"
#include "Project_Bang_Squad/Core/BSGameInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"

ABSGameMode::ABSGameMode()
{
}

// =========================================================================
// 코인 시스템 구현
// =========================================================================

void ABSGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	UBSGameInstance* GI = GetBSGameInstance();
	if (GI && NewPlayer)
	{
		if (ABSPlayerState* PS = NewPlayer->GetPlayerState<ABSPlayerState>())
		{
			// 복잡한 UniqueId 지우고 그냥 이름 쓰기
			FString UserKey = PS->GetPlayerName(); 
            
			int32 SavedCoin = GI->LoadCoinFromInstance(UserKey);
			PS->SetCoin(SavedCoin);
		}
	}
}

// 2. Logout 수정
void ABSGameMode::Logout(AController* Exiting)
{
	UBSGameInstance* GI = GetBSGameInstance();
	if (GI && Exiting)
	{
		if (ABSPlayerState* PS = Exiting->GetPlayerState<ABSPlayerState>())
		{
			// 이름 사용
			FString UserKey = PS->GetPlayerName();

			GI->SaveCoinToInstance(UserKey, PS->GetCoin());
		}
	}
	Super::Logout(Exiting);
}

void ABSGameMode::GiveStageClearReward(int32 Amount)
{
	if (!GameState)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("💰 보상 지급 시작! 인원수: %d"), GameState->PlayerArray.Num());

	for (APlayerState* PS : GameState->PlayerArray)
	{
		ABSPlayerState* BSPS = Cast<ABSPlayerState>(PS);
		if (BSPS)
		{
			BSPS->AddCoin(Amount);
			UE_LOG(LogTemp, Warning, TEXT("✅ %s 에게 %d 코인 지급 완료!"), *PS->GetPlayerName(), Amount);
		}
	}
	SaveAllPlayerCoins();
}

void ABSGameMode::GiveMiniGameReward(const TArray<APlayerController*>& RankedPlayers)
{
    TArray<int32> Rewards = {100, 70, 40, 20}; // 1~4등 보상
    
    for (int32 i = 0; i < RankedPlayers.Num(); i++)
    {
       if (i >= Rewards.Num()) break;
       
       if (APlayerController* PC = RankedPlayers[i])
       {
          if (ABSPlayerState* BSPS = PC->GetPlayerState<ABSPlayerState>())
          {
             BSPS->AddCoin(Rewards[i]);
             
             if (GEngine)
             {
                FString Msg = FString::Printf(TEXT("🏆 순위: %d등 | 보상: %d 코인 지급!"), i + 1, Rewards[i]);
                GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, Msg);
             }
                
             UE_LOG(LogTemp, Warning, TEXT("💰 [BSGameMode] Player(%s) finished Rank %d. Reward: %d Coin"), 
                *PC->GetName(), i + 1, Rewards[i]);
          }
       }
    }
    
    // 함수 이름 통일 (Players -> Player)
    SaveAllPlayerCoins();
    UE_LOG(LogTemp, Warning, TEXT("💾 [BSGameMode] All Coins Saved to GameInstance!"));
}

// 코인 저장 기능
void ABSGameMode::SaveAllPlayerCoins()
{
	UBSGameInstance* GI = GetBSGameInstance();
	if (!GI || !GameState) return;

	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (ABSPlayerState* BSPS = Cast<ABSPlayerState>(PS))
		{
			
			FString UserKey = BSPS->GetPlayerName();

			GI->SaveCoinToInstance(UserKey, BSPS->GetCoin());
		}
	}
}


void ABSGameMode::SpawnPlayerCharacter(AController* Controller, EJobType JobType)
{
    if (!Controller) return;
    UBSGameInstance* GI = GetBSGameInstance();
    if (!GI) return;
    
    TSubclassOf<ACharacter> PawnClass = GI->GetCharacterClass(JobType);
    if (!PawnClass) return;
    
    if (APawn* OldPawn = Controller->GetPawn())
    {
       OldPawn->Destroy();
    }

    FTransform SpawnTransform = GetRespawnTransform(Controller);

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    if (APawn* NewPawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform.GetLocation(), SpawnTransform.GetRotation().Rotator(), SpawnParams))
    {
       Controller->Possess(NewPawn);
    }
}

FTransform ABSGameMode::GetRespawnTransform(AController* Controller)
{
    AActor* PlayerStart = FindPlayerStart(Controller);
    if (PlayerStart)
    {
       return PlayerStart->GetActorTransform();
    }
    return FTransform::Identity;
}

void ABSGameMode::RequestRespawn(AController* Controller)
{
    if (!Controller) return;

    float WaitTime = GetRespawnDelay(Controller);

    if (ABSPlayerState* PS = Controller->GetPlayerState<ABSPlayerState>())
    {
       float ServerTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
       PS->SetRespawnEndTime(ServerTime + WaitTime);
    }
    
    FTimerHandle RespawnTimerHandle;
    FTimerDelegate RespawnDelegate;
    RespawnDelegate.BindUObject(this, &ABSGameMode::ExecuteRespawn, Controller);

    GetWorld()->GetTimerManager().SetTimer(RespawnTimerHandle, RespawnDelegate, WaitTime, false);
}


void ABSGameMode::ExecuteRespawn(AController* Controller)
{
    if (!Controller) return;

    EJobType JobToSpawn = EJobType::Titan;
    
    if (ABSPlayerState* PS = Controller->GetPlayerState<ABSPlayerState>())
    {
       JobToSpawn = PS->GetJob();
    }

    SpawnPlayerCharacter(Controller, JobToSpawn);
}

UBSGameInstance* ABSGameMode::GetBSGameInstance() const
{
    return Cast<UBSGameInstance>(GetGameInstance());
}