// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Project_Bang_Squad/Core/BSGameTypes.h" // 프로젝트 타입 정의 헤더
#include "BSPlayerController.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API ABSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	void RegisterManagedWidget(UUserWidget* InWidget);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ToggleShopUI();

	UFUNCTION(Server, Reliable)
	void Server_ReportQTEResult(AStage2Boss* TargetBoss, bool bSuccess);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	
	UFUNCTION(Server, Reliable)
	virtual void ServerSetNickName(const FString& NewName);

	/** 관리 중인 위젯 목록 (GC 방지 및 일괄 정리용) */
	//관리할 위젯 목록
	//UPROPERTY()
	//TArray<UUserWidget*> ManagedWidgets;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> ShopWidgetClass;

	UPROPERTY()
	class UShopMainWidget* ShopWidgetInstance;
	
	UPROPERTY()
	TArray<TObjectPtr<UUserWidget>> ManagedWidgets; // UE5 표준: Raw Pointer 대신 TObjectPtr 사용

#pragma region Debug Codes
public:
	UFUNCTION(Server, Reliable)
	void ServerDebugMoveToMapHandle(EStageIndex Stage, EStageSection Section);

	void MoveToStage1Map();
	void MoveToStage1MiniGameMap();
	void MoveToStage1BossMap();
	void MoveToStage2Map();
	void MoveToStage2MiniGameMap();
	void MoveToStage2BossMap();
	void MoveToStage3Map();
	void MoveToStage3MiniGameMap();
	void MoveToStage3BossMap();
#pragma endregion
};