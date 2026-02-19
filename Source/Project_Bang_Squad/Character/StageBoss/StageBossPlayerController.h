// Source/Project_Bang_Squad/Character/StageBoss/StageBossPlayerController.h
#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Game/Stage/StagePlayerController.h"
#include "InputActionValue.h"
#include "StageBossPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class AQTE_Trap; // 전방 선언

UCLASS()
class PROJECT_BANG_SQUAD_API AStageBossPlayerController : public AStagePlayerController
{
	GENERATED_BODY()

public:
	AStageBossPlayerController();

	// --- [스테이지 2 개인 QTE 로직 (서버 전용)] ---
	// 트랩이 플레이어를 가뒀을 때 컨트롤러에 등록
	void Server_SetQTETrap(AQTE_Trap* InTrap);
	// 트랩이 파괴되었을 때 컨트롤러에서 해제
	void Server_ClearQTETrap();

	// --- [스테이지 2 개인 QTE 로직 (클라이언트 UI)] ---
	// 트랩 진행도를 클라이언트 UI로 전달
	UFUNCTION(Client, Reliable)
	void Client_UpdateIndividualQTEUI(int32 CurrentCount, int32 MaxCount);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	// ==========================================
	// [기존] Stage 1 그룹 QTE (G키)
	// ==========================================
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|GroupQTE")
	TObjectPtr<UInputMappingContext> QTE_IMC;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|GroupQTE")
	TObjectPtr<UInputAction> QTE_Action;

	void Input_QTEInteract();

	UFUNCTION(Server, Reliable)
	void Server_SubmitQTEInput();


	// ==========================================
	// [신규] Stage 2 개인 QTE (A/D 연타)
	// ==========================================
	// 우선순위를 높게 설정하여 기존 이동을 덮어씌울 전용 매핑 컨텍스트
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|IndividualQTE")
	TObjectPtr<UInputMappingContext> IndividualQTE_IMC;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|IndividualQTE")
	TObjectPtr<UInputAction> IndividualQTE_Action;

	// 현재 나를 묶고 있는 트랩 (서버에서만 유효)
	UPROPERTY()
	TObjectPtr<AQTE_Trap> CurrentQTETrap;

	// [클라이언트] 입력 컨텍스트 교체 및 UI 표시/숨김
	UFUNCTION(Client, Reliable)
	void Client_ToggleIndividualQTEState(bool bIsActive);

	// [로컬] A/D 키 입력 시 호출
	void Input_IndividualQTEMash(const FInputActionValue& Value);

	// [서버] 서버의 트랩으로 진행도(탈출 시도) 전달
	UFUNCTION(Server, Reliable)
	void Server_SubmitIndividualQTEInput();

	// [블루프린트 연동] 개인 QTE UI 토글 이벤트
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|IndividualQTE")
	void OnToggleIndividualQTEWidget(bool bShow);

	// [블루프린트 연동] 개인 QTE UI 카운트 갱신 이벤트
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|IndividualQTE")
	void OnUpdateIndividualQTECount(int32 CurrentCount, int32 MaxCount);
};