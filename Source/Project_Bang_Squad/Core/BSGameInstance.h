// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BSGameTypes.h"
#include "Project_Bang_Squad/Core/SessionInterface.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Project_Bang_Squad/Game/Interface/SaveInterface.h"
#include "BSGameInstance.generated.h"

class UBSMapData;
class UBSJobData;
class UMainMenu;

USTRUCT()
struct FServerData
{
	GENERATED_BODY()

public:
	FString Name;
	uint16 CurrentPlayers;
	uint16 MaxPlayers;
	FString HostUserName;
};

UCLASS()
class PROJECT_BANG_SQUAD_API UBSGameInstance : public UGameInstance, public ISessionInterface
{
	GENERATED_BODY()

public:
	UBSGameInstance();

protected:
	virtual void Init() override;

public:
	void SetMainMenuWidget(UMainMenu* InMainMenu);

	UFUNCTION(BlueprintCallable)
	void LoadMainMenu();

#pragma region SessionInterface Codes
	virtual void Host(FString ServerName, int32 MaxPlayers, FString HostName) override;

	UFUNCTION(Exec)
	void Join(uint32 Index) override;

	UFUNCTION(Exec)
	void RefreshServerList() override;

	void OpenMainMenuLevel() override;
#pragma endregion

	// Host 호출 흐름 제어용 플래그
	bool bIsGoingToHost = false;

public:
	// ✅ 로비에서 사용할 방 이름 (Select UI 표시용)
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Lobby")
	FString LobbyRoomName;

private:
	// 세션 콜백들
	void OnCreateSessionComplete(FName InSessionName, bool IsSuccess);
	void OnDestroySessionComplete(FName InSessionName, bool IsSuccess);
	void OnFindSessionComplete(bool IsSuccess);
	void OnJoinSessionComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type InResult);
	void OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType,
	                      const FString& ErrorString);

	void CreateSession();

public:
	UFUNCTION(BlueprintCallable)
	void StartSession();

	// 원하는 최대 인원수 (Host 전에 세팅)
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	int32 DesiredMaxPlayers = 4;

private:
	// 메인 메뉴 / 일시정지 메뉴 위젯
	UPROPERTY(EditAnywhere)
	TSubclassOf<UUserWidget> MainMenuWidgetClass;
	class UMainMenu* MainMenu;

	// 세션 이름 / 호스트 이름
	FString DesiredServerName;
	FString DesiredHostName;

	// 온라인 세션
	IOnlineSessionPtr SessionInterface;
	TSharedPtr<FOnlineSessionSearch> SessionSearch;

	//최종 선택된 직업
	UPROPERTY()
	EJobType PlayerJob = EJobType::None;

public:
	FORCEINLINE EJobType GetPlayerJob() const { return PlayerJob; }
	void SetPlayerJob(EJobType NewJob) { PlayerJob = NewJob; }

	//닉네임 저장
	UPROPERTY()
	FString UserNickname;

#pragma region Stage Data Save

public:
	//죽은 몬스터의 고유 해시값(ID) 저장
	UPROPERTY()
	TSet<uint32> DeadMonsterIDs;

	//몬스터가 죽었을 때 ID 저장
	UFUNCTION()
	void MarkMonsterAsDead(uint32 ActorHash);

	UFUNCTION()
	bool IsMonsterDead(uint32 ActorHash) const;

	UFUNCTION()
	void ClearMonsterData();
#pragma endregion

#pragma region JobData
	UPROPERTY(EditDefaultsOnly, Category = "BS|Data")
	TObjectPtr<UBSJobData> JobDataAsset;
	UFUNCTION(BlueprintCallable, Category = "BS|Data")
	TSubclassOf<ACharacter> GetCharacterClass(EJobType InJobType) const;
	UFUNCTION(BlueprintCallable, Category = "BS|Data")
	UTexture2D* GetJobIcon(EJobType InJobType) const;
	UFUNCTION(BlueprintCallable, Category = "BS|Data")
	FLinearColor GetJobColor(EJobType InJobType) const;
#pragma endregion

#pragma region Map Data

public:
	UPROPERTY(EditDefaultsOnly, Category = "BS|Data")
	TObjectPtr<UBSMapData> MapDataAsset;

	UFUNCTION()
	void MoveToStage(EStageIndex InStage, EStageSection InSection);

protected:
	virtual void LoadComplete(const float LoadTime, const FString& MapName) override;
private:
	// 이동 중복 실행 방지용 플래그
	bool bIsTraveling = false;
#pragma endregion

#pragma region Portal

public:
	UPROPERTY()
	TSet<uint32> VisitedStageKeys;

	//방문한 거 표시
	UFUNCTION()
	void MarkStageAsVisited(EStageIndex Stage, EStageSection Section);

	//방문 했는지 확인
	UFUNCTION()
	bool HasVisitedStage(EStageIndex Stage, EStageSection Section) const;

	UFUNCTION()
	FORCEINLINE EStageIndex GetCurrentStage() const { return CurrentStage; }

	UFUNCTION()
	void SetCurrentStage(EStageIndex InStage) { CurrentStage = InStage; }

private:
	uint32 GetStageKey(EStageIndex Stage, EStageSection Section) const;

	UPROPERTY()
	EStageIndex CurrentStage = EStageIndex::None;
#pragma endregion

#pragma region Checkpoint

public:
	int32 GetSavedCheckpointIndex() const { return SavedCheckpointIndex; }
	void SetSavedCheckpointIndex(int32 NewIndex) { SavedCheckpointIndex = NewIndex; }
	void InitSavedCheckpointIndex() { SavedCheckpointIndex = 0; }

private:
	UPROPERTY()
	int32 SavedCheckpointIndex = 0;
#pragma endregion

#pragma region Cutscene Data
public:
	UPROPERTY(BlueprintReadWrite,EditAnywhere,Category = "BS|CutScene")
	bool bIsStage1CutscenePlayed = false;
#pragma endregion
	
public:
	//UFUNCTION()
	//FORCEINLINE EStageIndex GetCurrentStage() const { return CurrentStage; }
	//void SetCurrentStage(EStageIndex NewIndex) { CurrentStage = NewIndex; }


	// =========================================================================
	// 코인 시스템 (Coin System Persistence)
	// =========================================================================
public:
	// Key 타입을 FString으로 변경
	UPROPERTY()
	TMap<FString, int32> SavedPlayerCoins;

	// 코인 저장 (레벨 이동 전 호출)
	// [수정] 인자 타입 변경
	void SaveCoinToInstance(FString PlayerKey, int32 Amount)
	{
		SavedPlayerCoins.FindOrAdd(PlayerKey) = Amount;

		// 로그로 확인
		UE_LOG(LogTemp, Warning, TEXT("💾 [GameInstance] Saved for %s: %d G"), *PlayerKey, Amount);
	}

	//  코인 불러오기
	int32 LoadCoinFromInstance(FString PlayerKey)
	{
		if (SavedPlayerCoins.Contains(PlayerKey))
		{
			int32 Val = SavedPlayerCoins[PlayerKey];
			UE_LOG(LogTemp, Warning, TEXT("📂 [GameInstance] Loaded for %s: %d G"), *PlayerKey, Val);
			return Val;
		}
		UE_LOG(LogTemp, Warning, TEXT("📂 [GameInstance] No data for %s (Start 0 G)"), *PlayerKey);
		return 0;
		//return 1000;
	}
	
	// =========================================================================
	//  로딩 UI 시스템
	// =========================================================================
	UPROPERTY(EditAnywhere, Category = "UI|Loading")
	TSubclassOf<class UUserWidget> LoadingWidgetClass;
	
	// 스테이지 번호가 아닌, 맵 데이터에 있는 이미지 자체를 받는다.
	UFUNCTION(BlueprintCallable, Category = "UI|Loading")
	void ShowLoadingScreen(UTexture2D* LoadingImage);

#pragma region Save Actor

public:
	UPROPERTY()
	TMap<FName, FActorSaveData> StageSaveData;

	void SaveDataToInstance(FName ID, const FActorSaveData& Data) { StageSaveData.Add(ID, Data); }
	FActorSaveData* GetDataFromInstance(FName ID) { return StageSaveData.Find(ID); }

#pragma endregion
};
