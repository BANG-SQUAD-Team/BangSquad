// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/Core/BSGameInstance.h"
#include "Project_Bang_Squad/UI/Menu/MainMenu.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "Project_Bang_Squad/Data/DataAsset/BSJobData.h"
#include "Project_Bang_Squad/Data/DataAsset/BSMapData.h"
#include "Project_Bang_Squad/Game/Base/BSPlayerController.h"

const static FName SESSION_NAME = TEXT("GameSession");
const static FName SESSION_SETTINGS_KEY = TEXT("FREE");


UBSGameInstance::UBSGameInstance()
{
	ConstructorHelpers::FClassFinder<UUserWidget> MainMenuBPClass(TEXT("/Game/TeamShare/UI/WBP_MainMenu"));
	if (MainMenuBPClass.Succeeded())
		MainMenuWidgetClass = MainMenuBPClass.Class;
}

void UBSGameInstance::Init()
{
	Super::Init();

	IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
	if (OSS)
	{
		UE_LOG(LogTemp, Warning, TEXT("OSS : %s is Avaliable."), *OSS->GetSubsystemName().ToString());
		SessionInterface = OSS->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(
				this, &UBSGameInstance::OnCreateSessionComplete);
			SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(
				this, &UBSGameInstance::OnDestroySessionComplete);
			SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(this, &UBSGameInstance::OnFindSessionComplete);
			SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(this, &UBSGameInstance::OnJoinSessionComplete);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Not found subsystem."));
	}

	if (GEngine)
	{
		GEngine->OnNetworkFailure().AddUObject(this, &UBSGameInstance::OnNetworkFailure);
	}
}

void UBSGameInstance::SetMainMenuWidget(UMainMenu* InMainMenu)
{
	MainMenu = InMainMenu;
	if (MainMenu)
	{
		MainMenu->SetOwningInstance(this);
	}
}

void UBSGameInstance::LoadMainMenu()
{
	if (!ensure(MainMenuWidgetClass)) return;
	MainMenu = CreateWidget<UMainMenu>(this, MainMenuWidgetClass);
	if (!MainMenu) return;

	MainMenu->SetOwningInstance(this);
	MainMenu->StartUp();
}

void UBSGameInstance::Host(FString ServerName, int32 MaxPlayers, FString HostName)
{
	DesiredServerName = ServerName;
	DesiredMaxPlayers = MaxPlayers;
	DesiredHostName = HostName;

	// ✅ 로비에서 쓸 방 이름 저장 (Select UI에서 표시)
	LobbyRoomName = ServerName;

	// ✨ 방 만들기 시작! 깃발 세우기
	bIsGoingToHost = true;

	if (SessionInterface.IsValid())
	{
		auto AlreadyExsistingSession = SessionInterface->GetNamedSession(SESSION_NAME);
		if (AlreadyExsistingSession)
		{
			SessionInterface->DestroySession(SESSION_NAME);
		}
		else
		{
			CreateSession();
		}
	}
}

void UBSGameInstance::Join(uint32 Index)
{
	if (!SessionInterface.IsValid()) return;
	if (!SessionSearch.IsValid()) return;

	// 조인하러 떠날 때 위젯 포인터 끊기
	if (MainMenu)
	{
		MainMenu->Shutdown();
		MainMenu = nullptr;
	}

	if (SessionSearch->SearchResults.Num() > (int32)Index)
		SessionInterface->JoinSession(0, SESSION_NAME, SessionSearch->SearchResults[Index]);
	else
		UE_LOG(LogTemp, Warning, TEXT("Empty Session"));
}

void UBSGameInstance::RefreshServerList()
{
	bIsGoingToHost = false;

	if (SessionInterface.IsValid())
	{
		auto ExistingSession = SessionInterface->GetNamedSession(SESSION_NAME);
		if (ExistingSession)
		{
			SessionInterface->DestroySession(SESSION_NAME);
		}
	}

	SessionSearch = MakeShareable(new FOnlineSessionSearch());
	if (SessionSearch.IsValid())
	{
		SessionSearch->MaxSearchResults = 100;

		FString SubsystemName = IOnlineSubsystem::Get()->GetSubsystemName().ToString();

		if (SubsystemName == "NULL")
		{
			SessionSearch->bIsLanQuery = true;
			SessionSearch->QuerySettings.SearchParams.Empty(); // 필터 없이 다 찾음
		}
		else
		{
			// 스팀 설정
			SessionSearch->bIsLanQuery = false;
			SessionSearch->QuerySettings.Set(FName(TEXT("PRESENCESEARCH")), true, EOnlineComparisonOp::Equals);
		}

		SessionInterface->FindSessions(0, SessionSearch.ToSharedRef());
	}
}

void UBSGameInstance::OpenMainMenuLevel()
{
	APlayerController* PC = GetFirstLocalPlayerController();
	
	const FMapInfo* MapInfo = MapDataAsset->GetMapInfo(EStageIndex::Lobby, EStageSection::Menu);
	
	if (!PC || !MapInfo) return;
	FString Path = MapInfo->Level.GetLongPackageName();
	
	// 로비 전용 로딩 이미지
	ShowLoadingScreen(MapInfo->LoadingImage);
	
	FTimerHandle TravelTimer;
	GetWorld()->GetTimerManager().SetTimer(
		TravelTimer,
		[PC, Path]()
		{
			PC->ClientTravel(Path, ETravelType::TRAVEL_Absolute);
		},
		2.0f,
		false
		);
}

void UBSGameInstance::ShowLoadingScreen(UTexture2D* LoadingImage)
{
	// 위젯이 세팅 되어있고, 넘겨받은 이미지가 있을 때만 실행
	if (LoadingWidgetClass && LoadingImage && GetWorld())
	{
		if (UUserWidget* LoadingUI = CreateWidget<UUserWidget>(this, LoadingWidgetClass))
			// 위젯 블루프린트의 이벤트를 호출하여 이미지 전달
		{
			UFunction* Func =LoadingUI->FindFunction(FName("SetLoadingImage"));
			if (Func)
			{
				struct { UTexture2D* img; } Params;
				Params.img = LoadingImage;
				LoadingUI->ProcessEvent(Func, &Params);
			}
			
			// 화면 꽉 차게 제일 위에 띄움
			LoadingUI->AddToViewport(9999);
		}
			
	}
}

void UBSGameInstance::OnCreateSessionComplete(FName InSessionName, bool IsSuccess)
{
	if (!IsSuccess) return;
	
	MoveToStage(EStageIndex::Lobby, EStageSection::Main);
}

void UBSGameInstance::OnDestroySessionComplete(FName InSessionName, bool IsSuccess)
{
	if (IsSuccess && bIsGoingToHost)
	{
		CreateSession();
	}
}

void UBSGameInstance::OnFindSessionComplete(bool IsSuccess)
{
	// 위젯이 살아있는지 확인 (크래시 방지)
	if (MainMenu == nullptr || !MainMenu->IsValidLowLevel())
	{
		UE_LOG(LogTemp, Warning, TEXT("MainMenu가 유효하지 않습니다(Destroyed). 목록 갱신을 건너뜁니다."));
		return;
	}

	if (IsSuccess && SessionSearch.IsValid())
	{
		TArray<FServerData> ServerNames;
		for (const FOnlineSessionSearchResult& SearchResult : SessionSearch->SearchResults)
		{
			FServerData ServerData;
			ServerData.MaxPlayers = SearchResult.Session.SessionSettings.NumPublicConnections;
			ServerData.CurrentPlayers = ServerData.MaxPlayers - SearchResult.Session.NumOpenPublicConnections;

			// 기본값은 OwningUserName
			ServerData.HostUserName = SearchResult.Session.OwningUserName;

			// 방 이름
			FString ServerName;
			if (SearchResult.Session.SessionSettings.Get(SESSION_SETTINGS_KEY, ServerName))
				ServerData.Name = ServerName;

			// 닉네임("HOST_NAME") 꺼내오기
			FString HostName;
			if (SearchResult.Session.SessionSettings.Get(FName("HOST_NAME"), HostName))
			{
				ServerData.HostUserName = HostName;
			}
			else
			{
				if (ServerData.HostUserName.IsEmpty())
				{
					ServerData.HostUserName = TEXT("Unknown");
				}
			}

			ServerNames.Add(ServerData);
		}

		// 목록 갱신
		MainMenu->SetServerList(ServerNames);
		UE_LOG(LogTemp, Warning, TEXT("Finished Finding Session"));
	}
}

void UBSGameInstance::OnJoinSessionComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type InResult)
{
	if (!SessionInterface.IsValid()) return;

	if (InResult != EOnJoinSessionCompleteResult::Success)
	{
		UE_LOG(LogTemp, Error, TEXT("❌ 세션 접속 실패! 결과 코드: %d"), (int32)InResult);
		return;
	}

	FString Address;
	// 1. 주소 받아오기
	if (!SessionInterface->GetResolvedConnectString(InSessionName, Address))
	{
		UE_LOG(LogTemp, Error, TEXT("Could not convert IP Address"));
		return;
	}

	// 포트가 0번이면 17777로 강제 변경
	if (Address.EndsWith(":0"))
	{
		UE_LOG(LogTemp, Warning, TEXT("포트가 0번으로 감지됨. 17777로 강제 보정합니다."));
		Address = Address.Replace(TEXT(":0"), TEXT(":17777"));
	}

	UEngine* Engine = GetEngine();
	if (Engine)
	{
		Engine->AddOnScreenDebugMessage(0, 5, FColor::Green, FString::Printf(TEXT("Joining To %s"), *Address));
	}

	APlayerController* PC = GetFirstLocalPlayerController();
	if (PC)
	{
		if (MainMenu)
		{
			MainMenu->Shutdown();
			MainMenu = nullptr;
		}

		PC->ClientTravel(Address, ETravelType::TRAVEL_Absolute);
	}
}

void UBSGameInstance::OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType,
	const FString& ErrorString)
{
	OpenMainMenuLevel();
}

void UBSGameInstance::CreateSession()
{
	if (SessionInterface.IsValid())
	{
		FOnlineSessionSettings SessionSettings;

		// 서브시스템 이름 확인
		FString SubsystemName = IOnlineSubsystem::Get()->GetSubsystemName().ToString();

		if (SubsystemName == "NULL")
		{
			SessionSettings.bIsLANMatch = true;
			SessionSettings.bUsesPresence = false; // LAN 설정
		}
		else
		{
			SessionSettings.bIsLANMatch = false;
			SessionSettings.bUsesPresence = true; // 스팀 설정
		}

		// 인원수 적용
		SessionSettings.NumPublicConnections = DesiredMaxPlayers;
		SessionSettings.bShouldAdvertise = true;

		// 방 이름 저장
		SessionSettings.Set(SESSION_SETTINGS_KEY, DesiredServerName,
							EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

		// ✨ 닉네임 저장! ("HOST_NAME" 키)
		SessionSettings.Set(FName("HOST_NAME"), DesiredHostName,
							EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

		// 메인 메뉴 위젯 정리
		if (MainMenu)
		{
			MainMenu->Shutdown();
			MainMenu = nullptr;
		}

		SessionInterface->CreateSession(0, SESSION_NAME, SessionSettings);
	}
}

void UBSGameInstance::StartSession()
{
	if (SessionInterface.IsValid())
		SessionInterface->StartSession(SESSION_NAME);
}

void UBSGameInstance::MarkMonsterAsDead(uint32 ActorHash)
{
	DeadMonsterIDs.Add(ActorHash);
}

bool UBSGameInstance::IsMonsterDead(uint32 ActorHash) const
{
	return DeadMonsterIDs.Contains(ActorHash);
}

void UBSGameInstance::ClearMonsterData()
{
	DeadMonsterIDs.Empty();
}

TSubclassOf<ACharacter> UBSGameInstance::GetCharacterClass(EJobType InJobType) const
{
	if (JobDataAsset)
	{
		return JobDataAsset->GetCharacterClass(InJobType);
	}
	return nullptr;
}

UTexture2D* UBSGameInstance::GetJobIcon(EJobType InJobType) const
{
	if (JobDataAsset)
	{
		return JobDataAsset->GetJobIcon(InJobType);
	}
	return nullptr;
}

FLinearColor UBSGameInstance::GetJobColor(EJobType InJobType) const
{
	if (JobDataAsset)
	{
		return JobDataAsset->GetJobColor(InJobType);
	}
	return FLinearColor::White;
}

void UBSGameInstance::MoveToStage(EStageIndex InStage, EStageSection InSection)
{
	if (!MapDataAsset) return;

	if (GetCurrentStage() != InStage)
	{
		InitSavedCheckpointIndex();
		SetCurrentStage(InStage);
		ClearMonsterData();
	}

	const FMapInfo* MapInfo = MapDataAsset->GetMapInfo(InStage, InSection);
    
	if (MapInfo && !MapInfo->Level.IsNull() && GetWorld())
	{
		FString Path = MapInfo->Level.GetLongPackageName();

		//  서버가 모든 플레이어에게 '맵 정보(Enum)'만 가볍게 전달
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (ABSPlayerController* PC = Cast<ABSPlayerController>(It->Get()))
			{
				// 변경된 부분: 이미지 포인터 대신 InStage, InSection 전달
				PC->Client_ShowLoadingScreen(InStage, InSection); 
			}
		}
		
		FTimerHandle TravelTimer;
		GetWorld()->GetTimerManager().SetTimer(
			TravelTimer, 
			[this, Path]()
			{
				GetWorld()->ServerTravel(Path + "?listen");
			}, 
			2.0f, 
			false
		);
	}
}

void UBSGameInstance::MarkStageAsVisited(EStageIndex Stage, EStageSection Section)
{
	uint32 Key = GetStageKey(Stage, Section);
	VisitedStageKeys.Add(Key);
}

bool UBSGameInstance::HasVisitedStage(EStageIndex Stage, EStageSection Section) const
{
	uint32 Key = GetStageKey(Stage, Section);
	return VisitedStageKeys.Contains(Key);
}

uint32 UBSGameInstance::GetStageKey(EStageIndex Stage, EStageSection Section) const
{
	//비트 연산으로 두 값 섞어서 고유 ID 생성
	return ((uint32)Stage << 16) | (uint32)Section;
}
