// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/Game/Stage/StagePlayerController.h"

#include "EnhancedInputComponent.h"
#include "StageGameMode.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Project_Bang_Squad/Core/BSGameInstance.h"

void AStagePlayerController::BeginPlay()
{
	Super::BeginPlay();

	//Local인 경우에만
	if (IsLocalPlayerController())
	{
		FInputModeGameOnly GameInputMode;
		SetInputMode(GameInputMode);
		bShowMouseCursor = false;
		
		//GameInstance에서 내가 고른 직업 꺼내오기
		if (UBSGameInstance* GI = Cast<UBSGameInstance>(GetGameInstance()))
		{
			//서버에게 소환 요청
			ServerRequestSpawn(GI->GetMyJob());
		}
	}
}

void AStagePlayerController::StartSpectating()
{
	ViewNextPlayer();
}

void AStagePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// 관전 키 바인딩 (죽었을 때만 작동하도록 함수 내부에서 체크)
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (IA_SpectateNext)
		{
			EIC->BindAction(IA_SpectateNext, ETriggerEvent::Started, this, &AStagePlayerController::ViewNextPlayer);
		}
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

void AStagePlayerController::ServerRequestSpawn_Implementation(EJobType MyJob)
{
	//서버에서 게임모드를 찾아 실제 소환명령
	if (AStageGameMode* GM = GetWorld()->GetAuthGameMode<AStageGameMode>())
	{
		GM->SpawnPlayerCharacter(this, MyJob);
	}
}


