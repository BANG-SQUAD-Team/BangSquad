// BSPlayerState.cpp

#include "Project_Bang_Squad/Game/Base/BSPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"

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
	DOREPLIFETIME(ABSPlayerState, OwnedItemIds);

	DOREPLIFETIME(ABSPlayerState, CurrentEquippedHeadID);
	DOREPLIFETIME(ABSPlayerState, CurrentEquippedSkinID);
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

void ABSPlayerState::OnRep_EquippedItems()
{
	// 내 캐릭터를 찾아서 외형 업데이트 요청
	if (APawn* MyPawn = GetPawn())
	{
		if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(MyPawn))
		{
			// 캐릭터에게 PlayerState의 정보를 읽어서 다시 입으라고 시킴
			BaseChar->UpdateAppearanceFromPlayerState();
		}
	}
}

// 서버에서 장착 처리
void ABSPlayerState::Server_EquipItem_Implementation(FName ItemID, EItemType ItemType)
{
	if (ItemType == EItemType::HeadGear)
	{
		CurrentEquippedHeadID = ItemID;
	}
	else if (ItemType == EItemType::Skin)
	{
		CurrentEquippedSkinID = ItemID;
	}

	// 서버에서도 즉시 갱신 (서버가 Host인 경우 OnRep이 안 불리므로)
	OnRep_EquippedItems();
}

void ABSPlayerState::Server_TryPurchase_Implementation(FName ItemID, int32 Cost, EItemType ItemType)
{
	if (CoinAmount >= Cost)
	{
		AddCoin(-Cost);
		AddItem(ItemID); // 소유 목록 추가

		OnPurchaseResult.Broadcast(true);

		// ★ [추가] 구매했으면 바로 장착까지 해줍니다! (편의성)
		Server_EquipItem(ItemID, ItemType);
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