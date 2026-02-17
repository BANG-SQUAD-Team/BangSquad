// BSPlayerState.cpp

#include "Project_Bang_Squad/Game/Base/BSPlayerState.h"
#include "Net/UnrealNetwork.h"

// --- [기존] 직업 및 리스폰 로직 ---
void ABSPlayerState::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		SetCoin(10000); // 테스트용: 1만 골드 지급
	}
}

void ABSPlayerState::OnRep_JobType()
{
	// UI 업데이트 로직 (필요 시 구현)
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
	if (ABSPlayerState* NewPS = Cast<ABSPlayerState>(PlayerState))
	{
		NewPS->JobType = JobType;
		NewPS->RespawnEndTime = RespawnEndTime;
		NewPS->CoinAmount = CoinAmount;
		NewPS->OwnedItemIds = OwnedItemIds; // 아이템 목록도 복사
	}
}

void ABSPlayerState::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABSPlayerState, JobType);
	DOREPLIFETIME(ABSPlayerState, RespawnEndTime);
	DOREPLIFETIME(ABSPlayerState, CoinAmount);
	// ★ [중요] 아이템 목록 동기화 등록
	DOREPLIFETIME(ABSPlayerState, OwnedItemIds);
}

void ABSPlayerState::Server_SetJob_Implementation(EJobType NewJob)
{
	JobType = NewJob;
}

// --- [기존] 코인 로직 ---
void ABSPlayerState::AddCoin(int32 Amount)
{
	if (HasAuthority())
	{
		CoinAmount += Amount;
		OnCoinChanged.Broadcast(CoinAmount);
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

// =========================================================================
// ★ [신규] 인벤토리 및 판매 시스템 구현
// =========================================================================

bool ABSPlayerState::HasItem(FName ItemID) const
{
	return OwnedItemIds.Contains(ItemID);
}

void ABSPlayerState::AddItem(FName ItemID)
{
	if (!OwnedItemIds.Contains(ItemID))
	{
		OwnedItemIds.Add(ItemID);
	}
}

void ABSPlayerState::RemoveItem(FName ItemID)
{
	if (OwnedItemIds.Contains(ItemID))
	{
		OwnedItemIds.Remove(ItemID);
	}
}

// 구매 (기존 함수 수정: 성공 시 아이템 추가 로직은 ItemID를 인자로 받아야 하므로, 
// 지금은 돈만 깎는 로직 유지하되 필요하면 수정하세요)
void ABSPlayerState::Server_TryPurchase_Implementation(int32 TotalCost)
{
	if (!HasAuthority()) return;

	if (CoinAmount >= TotalCost)
	{
		AddCoin(-TotalCost);
		OnPurchaseResult.Broadcast(true);

		// [참고] 실제로 아이템을 주려면 여기서 AddItem(ItemID)를 해야 하는데,
		// 현재 함수 인자에는 ItemID가 없으므로 생략합니다. 
		// (UI 테스트 목적이라면 "돈이 깎이고 성공 메시지가 오는 것"만으로 충분합니다)
	}
	else
	{
		OnPurchaseResult.Broadcast(false);
	}
}

// ★ 판매 (이게 없어서 오류가 났었습니다)
void ABSPlayerState::Server_TrySell_Implementation(FName ItemID, int32 SellAmount)
{
	if (!HasAuthority()) return;

	// 1. 보유 확인
	if (HasItem(ItemID))
	{
		// 2. 아이템 삭제
		RemoveItem(ItemID);

		// 3. 돈 지급
		AddCoin(SellAmount);

		// 4. 성공 알림
		OnPurchaseResult.Broadcast(true);

		UE_LOG(LogTemp, Warning, TEXT("판매 성공! ID: %s, 획득: %d G"), *ItemID.ToString(), SellAmount);
	}
	else
	{
		// 가지고 있지도 않은데 팔려고 함 (해킹 방지 등)
		UE_LOG(LogTemp, Error, TEXT("판매 실패! 미보유 아이템: %s"), *ItemID.ToString());
		OnPurchaseResult.Broadcast(false);
	}
}