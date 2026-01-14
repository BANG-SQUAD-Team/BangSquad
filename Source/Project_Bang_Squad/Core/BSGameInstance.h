// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Core/SessionInterface.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "BSGameInstance.generated.h"

class UMainMenu;

UENUM(BlueprintType)
enum class EJobType : uint8
{
	None,
	Titan,
	Striker,
	Mage,
	Paladin
};

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
	// 내가 고른 캐릭터 ID 저장
	UPROPERTY(BlueprintReadWrite)
	int32 MyCharacterID = 1;

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
	EJobType MyJob = EJobType::None;
public:
	FORCEINLINE EJobType GetMyJob() const { return MyJob; }
	void SetMyJob(EJobType NewJob) { MyJob = NewJob; }

	//닉네임 저장
	UPROPERTY()
	FString UserNickname;
};
