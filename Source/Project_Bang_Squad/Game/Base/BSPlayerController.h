// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Project_Bang_Squad/Core/BSGameTypes.h" // 프로젝트 타입 정의 헤더
#include "BSPlayerController.generated.h"

// [전방 선언] 헤더 의존성 최소화 및 컴파일 속도 향상
class UInputMappingContext;
class UInputAction;
class UUserWidget;
class UQTEWidget;
class AStage2Boss;

/**
 * ABSPlayerController
 * * [역할]
 * 1. UI (위젯) 관리
 * 2. 네트워크 플레이어 정보 동기화 (닉네임 등)
 * 3. 디버그 및 치트 기능 (서버 검증 포함)
 * 4. 보스전 특수 기믹 (QTE) 입력 처리
 */
UCLASS()
class PROJECT_BANG_SQUAD_API ABSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ABSPlayerController();

	// --- [Core: Widget Management] ---
	/** 외부에서 위젯을 등록하여 PC가 수명 주기를 관리하게 함 */
	void RegisterManagedWidget(UUserWidget* InWidget);

protected:
	// --- [Core: Lifecycle] ---
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override; // Enhanced Input 바인딩 위치

	// --- [Network: Player Info] ---
	/** * 닉네임 변경 요청 (Server RPC)
	 * - 이유: 닉네임은 다른 클라이언트에게도 보여야 하므로 서버가 관리해야 함 (Replication)
	 */
	UFUNCTION(Server, Reliable)
	void ServerSetNickName(const FString& NewName);

	/** 관리 중인 위젯 목록 (GC 방지 및 일괄 정리용) */
	UPROPERTY()
	TArray<TObjectPtr<UUserWidget>> ManagedWidgets; // UE5 표준: Raw Pointer 대신 TObjectPtr 사용

public:
	// =============================================================
	// [Boss QTE System] - 보스전 연타 기믹
	// =============================================================

	/** * [Client RPC] 보스가 특정 플레이어에게 QTE 시작을 명령
	 * @param BossActor : 결과를 보고할 보스 인스턴스 (서버로 RPC를 보내기 위해 필요)
	 * @param TargetCount : 성공에 필요한 연타 횟수
	 * @param Duration : 제한 시간
	 */
	UFUNCTION(Client, Reliable)
	void Client_StartQTE(AStage2Boss* BossActor, int32 TargetCount, float Duration);

	/** [Client RPC] QTE 종료 처리 (UI 제거 및 입력 모드 복구) */
	UFUNCTION(Client, Reliable)
	void Client_EndQTE(bool bSuccess);

protected:
	/** QTE 입력 액션 바인딩 함수 (A/D 연타) */
	void OnQTETapInput();

	// --- [QTE Configuration] (BP에서 할당) ---

	/** QTE 전용 입력 컨텍스트 (Priority를 높게 설정하여 이동 입력을 덮어써야 함) */
	UPROPERTY(EditDefaultsOnly, Category = "Input|QTE")
	TObjectPtr<UInputMappingContext> QTE_Context;

	/** 연타 액션 (A, D키 등) */
	UPROPERTY(EditDefaultsOnly, Category = "Input|QTE")
	TObjectPtr<UInputAction> QTE_TapAction;

	/** QTE UI 위젯 클래스 (WBP_QTE) */
	UPROPERTY(EditDefaultsOnly, Category = "UI|QTE")
	TSubclassOf<UQTEWidget> QTEWidgetClass;

private:
	// --- [QTE Internal State] ---

	/** 현재 생성된 QTE 위젯 인스턴스 */
	UPROPERTY()
	TObjectPtr<UQTEWidget> QTEWidgetInstance;

	/** QTE를 발동시킨 보스 참조 (Strong Ref로 GC 방지) */
	UPROPERTY()
	TObjectPtr<AStage2Boss> CurrentBossRef;

	int32 CurrentTapCount; // 현재 연타 횟수
	int32 GoalTapCount;    // 목표 연타 횟수
	bool bIsQTEActive;     // 중복 입력 방지 플래그

#pragma region Debug Codes
public:
	// =============================================================
	// [Debug & Cheat] - 개발 편의 기능
	// =============================================================

	/** * [Server RPC] 맵 이동 요청 처리
	 * - 이유: 맵 이동(Travel)은 반드시 서버 권한(Authority)이 필요함
	 */
	UFUNCTION(Server, Reliable)
	void ServerDebugMoveToMapHandle(EStageIndex Stage, EStageSection Section);

	// 클라이언트에서 호출하는 래퍼 함수들
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