// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/Game/Stage/StagePlayerController.h"

#include "EnhancedInputComponent.h"
#include "StageGameMode.h"
#include "StagePlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Project_Bang_Squad/Core/BSGameInstance.h"
#include "Project_Bang_Squad/Game/InteractionInterface.h"
#include "Project_Bang_Squad/Game/MiniGame/MiniGameMode.h"
#include "Project_Bang_Squad/UI/Stage/StageMainWidget.h"

void AStagePlayerController::BeginPlay()
{
	Super::BeginPlay();

	//Local인 경우에만
	if (IsLocalPlayerController())
	{
		// 캐릭터 밝기 조절
		if (WorldSettingsMPC)
		{
			FString MapName = GetWorld()->GetMapName();
			float TargetEmissive = 1.5f; // 기본 밝기

			if (MapName.Contains(TEXT("Stage2")))
			{
				TargetEmissive = 0.1f;
			}

			UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), WorldSettingsMPC, TEXT("GlobalCharacterEmissive"), TargetEmissive);
		}

		//UI
		if (StageMainWidgetClass)
		{
			StageMainWidget = CreateWidget<UStageMainWidget>(this, StageMainWidgetClass);
			if (StageMainWidget)
			{
				StageMainWidget->SetVisibility(ESlateVisibility::Visible);
				StageMainWidget->AddToViewport();
			}
		}

		FInputModeGameOnly GameInputMode;
		SetInputMode(GameInputMode);
		bShowMouseCursor = false;

		//GameInstance에서 내가 고른 직업 꺼내오기
		if (UBSGameInstance* GI = Cast<UBSGameInstance>(GetGameInstance()))
		{
			ServerSetNickName(GI->UserNickname);
			//서버에게 소환 요청
			ServerRequestSpawn(GI->GetMyJob());
		}
	}
}

void AStagePlayerController::ServerSetNickName_Implementation(const FString& InNickName)
{
	if (PlayerState)
	{
		PlayerState->SetPlayerName(InNickName);
	}
}

void AStagePlayerController::StartSpectating()
{
	ViewNextPlayer();

	if (HasAuthority())
	{
		// StageGameMode 체크
		if (AStageGameMode* StageGM = GetWorld()->GetAuthGameMode<AStageGameMode>())
		{
			StageGM->RequestRespawn(this);
		}
		// MiniGameMode 체크
		else if (AMiniGameMode* MiniGM = GetWorld()->GetAuthGameMode<AMiniGameMode>())
		{
			MiniGM->RequestRespawn(this);
		}
	}
}

void AStagePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// 관전 키 바인딩 (죽었을 때만 작동하도록 함수 내부에서 체크)
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (IA_SpectateNext)
			EIC->BindAction(IA_SpectateNext, ETriggerEvent::Started, this, &AStagePlayerController::ViewNextPlayer);
		if (IA_Interact)
			EIC->BindAction(IA_Interact, ETriggerEvent::Started, this, &AStagePlayerController::OnInputInteract);
	}
}

void AStagePlayerController::ViewNextPlayer()
{
	// 내가 살아있다면 관전 기능 무시
	ABaseCharacter* MyPawn = Cast<ABaseCharacter>(GetPawn());
	if (MyPawn && !MyPawn->IsDead()) return;

	// 1. GameState에서 전체 플레이어 목록 가져오기
	AGameStateBase* GS = GetWorld()->GetGameState();
	if (!GS) return;

	TArray<ABaseCharacter*> AlivePlayers;

	// 2. 살아있는 동료만 필터링
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS || PS == PlayerState) continue; // 나 자신은 제외

		ABaseCharacter* Char = Cast<ABaseCharacter>(PS->GetPawn());
		// 캐릭터가 존재하고 살아있다면 리스트에 추가
		if (Char && !Char->IsDead())
		{
			AlivePlayers.Add(Char);
		}
	}

	if (AlivePlayers.IsEmpty()) return; // 관전할 사람이 없음

	// 3. 현재 보고 있는 대상의 인덱스 찾기
	AActor* CurrentViewTarget = GetViewTarget();
	int32 CurrentIndex = -1;

	for (int32 i = 0; i < AlivePlayers.Num(); ++i)
	{
		if (AlivePlayers[i] == CurrentViewTarget)
		{
			CurrentIndex = i;
			break;
		}
	}

	// 4. 다음 순번 계산 (순환 구조)
	int32 NextIndex = (CurrentIndex + 1) % AlivePlayers.Num();

	// 5. 시점 변경 (핵심 기능)
	// 0.5초 동안 부드럽게 카메라가 이동하며, 상대방의 화면을 그대로 공유함
	SetViewTargetWithBlend(AlivePlayers[NextIndex], 0.5f, VTBlend_Cubic);
}

void AStagePlayerController::OnInputInteract()
{
	Server_Interact();
}

void AStagePlayerController::Server_Interact_Implementation()
{
	if (!HasAuthority()) return;
	
	APawn* MyPawn = GetPawn();
	if (!MyPawn) return;

	FVector Start = MyPawn->GetActorLocation();
	FVector End = Start;
	float Radius = 150.f;

	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(MyPawn);

	TArray<AActor*> OutActors;

	UKismetSystemLibrary::SphereOverlapActors(
		GetWorld(),
		Start,
		Radius,
		ObjectTypes,
		nullptr,
		ActorsToIgnore,
		OutActors
	);

	for (AActor* Actor : OutActors)
	{
		if (Actor && Actor->Implements<UInteractionInterface>())
		{
			IInteractionInterface::Execute_Interact(Actor, MyPawn);

			return;
		}
	}
}

void AStagePlayerController::ServerRequestSpawn_Implementation(EJobType MyJob)
{
	SavedJobType = MyJob;

	if (AStagePlayerState* PS = GetPlayerState<AStagePlayerState>())
	{
		PS->SetJob(MyJob);
	}
	AGameModeBase* AutoGM = GetWorld()->GetAuthGameMode();

	//서버에서 게임모드를 찾아 실제 소환명령
	if (AStageGameMode* StageGM = Cast<AStageGameMode>(AutoGM))
	{
		StageGM->SpawnPlayerCharacter(this, MyJob);
	}
	else if (AMiniGameMode* MiniGM = Cast<AMiniGameMode>(AutoGM))
	{
		MiniGM->SpawnPlayerCharacter(this, MyJob);
	}
}
