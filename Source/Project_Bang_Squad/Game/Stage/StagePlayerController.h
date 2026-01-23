// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "StagePlayerController.generated.h"

class UStageMainWidget;
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

	//서버에게 닉네임 알리기
	UFUNCTION(Server, Reliable)
	void ServerSetNickName(const FString& InNickName);

	//캐릭터 사망 시 호출
	void StartSpectating();

protected:
	virtual void SetupInputComponent() override;

	//관전 대상 변경 함수
	void ViewNextPlayer();

	//에디터에서 할당할 입력 액션 (스페이스바 등)
	UPROPERTY(EditDefaultsOnly, Category = "BS|Input")
	class UInputAction* IA_SpectateNext;

	//상호작용 키
	UPROPERTY(EditDefaultsOnly, Category = "BS|Input")
	class UInputAction* IA_Interact;

	//입력 감지
	void OnInputInteract();
	//서버에게 요청
	UFUNCTION(Server, Reliable)
	void Server_Interact();

public:
	UPROPERTY(BlueprintReadOnly)
	EJobType SavedJobType;

	FORCEINLINE int32 GetDeathCount() const { return DeathCount; };

	void IncreaseDeathCount() { DeathCount++; }

	//첫 스폰 여부
	bool bHasSpawnedOnce = false;

private:
	UPROPERTY()
	int32 DeathCount = 0;

	UPROPERTY(EditDefaultsOnly, Category = "BS|UI")
	TSubclassOf<UStageMainWidget> StageMainWidgetClass;

	UPROPERTY()
	TObjectPtr<UStageMainWidget> StageMainWidget;

	// 에디터에서 생성한 MPC 에셋을 넣을 변수
	UPROPERTY(EditDefaultsOnly, Category = "BS|Visual")
	TObjectPtr<class UMaterialParameterCollection> WorldSettingsMPC;
};
