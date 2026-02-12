// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/Game/Base/BSPlayerState.h"

#include "Net/UnrealNetwork.h"

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
	ABSPlayerState* PS = Cast<ABSPlayerState>(PlayerState);
	if (PS)
	{
		PS->JobType = JobType;
		PS->RespawnEndTime = RespawnEndTime;
	}
}

void ABSPlayerState::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABSPlayerState, JobType);
	DOREPLIFETIME(ABSPlayerState, RespawnEndTime);
	DOREPLIFETIME(ABSPlayerState, CoinAmount);
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