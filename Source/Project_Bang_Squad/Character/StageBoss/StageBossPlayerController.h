// Source/Project_Bang_Squad/Character/StageBoss/StageBossPlayerController.h

#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Game/Stage/StagePlayerController.h"
#include "InputActionValue.h"
#include "StageBossPlayerController.generated.h"

class UQTEWidget;
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
	void Server_SetQTETrap(AQTE_Trap* InTrap);
	void Server_ClearQTETrap();

	// --- [스테이지 2 개인 QTE 로직 (클라이언트 UI)] ---
	UFUNCTION(Client, Reliable)
	void Client_UpdateIndividualQTEUI(int32 CurrentCount, int32 MaxCount);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override; // [복구] 기존 EndPlay
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

	// [복구] 기존 QTE 위젯 인스턴스
	UPROPERTY()
	TObjectPtr<UQTEWidget> QTEWidgetInstance;

	// ==========================================
	// [신규] Stage 2 개인 QTE (A/D 연타)
	// ==========================================
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|IndividualQTE")
	TObjectPtr<UInputMappingContext> IndividualQTE_IMC;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|IndividualQTE")
	TObjectPtr<UInputAction> IndividualQTE_Action;

	UPROPERTY()
	TObjectPtr<AQTE_Trap> CurrentQTETrap;

	UFUNCTION(Client, Reliable)
	void Client_ToggleIndividualQTEState(bool bIsActive);

	void Input_IndividualQTEMash(const FInputActionValue& Value);

	UFUNCTION(Server, Reliable)
	void Server_SubmitIndividualQTEInput();

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|IndividualQTE")
	void OnToggleIndividualQTEWidget(bool bShow);

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|IndividualQTE")
	void OnUpdateIndividualQTECount(int32 CurrentCount, int32 MaxCount);
};