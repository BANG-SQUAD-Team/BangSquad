// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "StagePlayerController.generated.h"

enum class EJobType : uint8;
/**
 * 
 */
UCLASS()
class PROJECT_BANG_SQUAD_API AStagePlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

public:
	//서버에게 소환해달라고 요청하는 함수
	UFUNCTION(Server, Reliable)
	void ServerRequestSpawn(EJobType MyJob);

	//캐릭터 사망 시 호출
	void StartSpectating();

protected:
	virtual void SetupInputComponent() override;

	// 관전 대상 변경 함수
	void ViewNextPlayer();

	// 에디터에서 할당할 입력 액션 (스페이스바 등)
	UPROPERTY(EditDefaultsOnly, Category = "BS|Input")
	class UInputAction* IA_SpectateNext;
};
