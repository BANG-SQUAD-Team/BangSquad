// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/Game/Base/BSPlayerState.h"

#include "Net/UnrealNetwork.h"

void ABSPlayerState::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		// 1. 맵 이동 후: 넘어온 이름이 있으면걸로 복구!
		if (!FixedPlayerName.IsEmpty())
		{
			// 엔진이 맘대로 바꾼 이름(DESKTOP...)을 버리고 내 진짜 이름으로 강제 설정
			SetPlayerName(FixedPlayerName);
		}
		// 2. 게임 최초 접속 (로비): 아직 FixedPlayerName이 없으니 현재 이름으로 저장
		else
		{
			FString CurrentName = GetPlayerName();
            
			// 만약 이름이 비어있거나 DESKTOP이면 임시 이름 부여 (안전장치)
			if (CurrentName.IsEmpty() || CurrentName.Contains(TEXT("DESKTOP")))
			{
				CurrentName = FString::Printf(TEXT("Player_%d"), FMath::RandRange(100, 999));
				SetPlayerName(CurrentName);
			}
            
			// 확정된 이름을 '고정 이름' 변수에 박제!
			FixedPlayerName = CurrentName;
		}
        
		// 이름 확정 후 코인 UI 갱신 (혹시 몰라 한 번 더 호출)
		OnCoinChanged.Broadcast(CoinAmount);
	}
}

void ABSPlayerState::OnRep_JobType()
{
	//TODO: UI 업데이트 로직
}

void ABSPlayerState::OnRep_PlayerName()
{
	Super::OnRep_PlayerName();
}

void ABSPlayerState::SetJob(EJobType NewJob)
{
	if (HasAuthority())
	{
		JobType = NewJob;
		OnRep_JobType();
	}
}

void ABSPlayerState::SetRespawnEndTime(float NewTime)
{
	RespawnEndTime = NewTime;
}

void ABSPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	//다음 Level의 PlayerState로 직업 정보 복사
	ABSPlayerState* NewPS = Cast<ABSPlayerState>(PlayerState);
	if (NewPS)
	{
		NewPS->JobType = JobType;
		NewPS->RespawnEndTime = RespawnEndTime;
		NewPS->CoinAmount = CoinAmount;
		// 맵 이동 직전에, 현재 내 이름이 진짜라면 FixedPlayerName을 최신화합니다
		// (로비에서 바꾼 이름이 아직 FixedPlayerName에 안 들어갔을 수도 있으니까요)
		FString CurrentName = GetPlayerName();
		if (!CurrentName.IsEmpty() && !CurrentName.Contains(TEXT("DESKTOP")))
		{
			FixedPlayerName = CurrentName;
		}

		// 최신화된 고정 이름을 다음 맵으로 넘김
		NewPS->FixedPlayerName = FixedPlayerName;
	}
}

void ABSPlayerState::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABSPlayerState, JobType);
	DOREPLIFETIME(ABSPlayerState, RespawnEndTime);
	DOREPLIFETIME(ABSPlayerState, CoinAmount);
	DOREPLIFETIME(ABSPlayerState, FixedPlayerName);
}

void ABSPlayerState::Server_SetJob_Implementation(EJobType NewJob)
{
	JobType = NewJob;
}

// =========================================================================
// 코인 시스템 구현
// =========================================================================

void ABSPlayerState::AddCoin(int32 Amount)
{
	if (HasAuthority())
	{
		CoinAmount += Amount;
		OnCoinChanged.Broadcast(CoinAmount); // 서버 UI 갱신
		
		if (GEngine)
		{
			FString Msg = FString::Printf(TEXT("✅ 코인 획득! (+%d) | 현재 총액: %d"), Amount, CoinAmount);
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, Msg);
		}
	}
}

void ABSPlayerState::SetCoin(int32 Amount)
{
	if (HasAuthority())
	{
		CoinAmount = Amount;
		OnCoinChanged.Broadcast(CoinAmount);
	}
}

void ABSPlayerState::OnRep_CoinAmount()
{
	OnCoinChanged.Broadcast(CoinAmount);
}