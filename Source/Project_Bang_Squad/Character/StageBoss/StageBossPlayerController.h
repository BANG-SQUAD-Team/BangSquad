// Source/Project_Bang_Squad/Character/StageBoss/StageBossPlayerController.h

#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Game/Stage/StagePlayerController.h"
#include "InputActionValue.h"
#include "StageBossPlayerController.generated.h"

class UQTEWidget;
class UInputMappingContext;
class UInputAction;
class AQTE_Trap;
class UADQTEWidget;
class UGQTEWidget;

UCLASS()
class PROJECT_BANG_SQUAD_API AStageBossPlayerController : public AStagePlayerController
{
	GENERATED_BODY()

public:
	AStageBossPlayerController();

	// --- [�������� 2 ���� QTE ���� (���� ����)] ---
	void Server_SetQTETrap(AQTE_Trap* InTrap);
	void Server_ClearQTETrap();

	// --- [�������� 2 ���� QTE ���� (Ŭ���̾�Ʈ UI)] ---
	UFUNCTION(Client, Reliable)
	void Client_UpdateIndividualQTEUI(int32 CurrentCount, int32 MaxCount);

	UFUNCTION(Client, Reliable)
	void Client_ToggleGroupQTEUI(bool bShow);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override; // [����] ���� EndPlay
	virtual void SetupInputComponent() override;

	// ==========================================
	// [����] Stage 1 �׷� QTE (GŰ)
	// ==========================================
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|GroupQTE")
	TObjectPtr<UInputMappingContext> QTE_IMC;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|GroupQTE")
	TObjectPtr<UInputAction> QTE_Action;

	void Input_QTEInteract();

	UFUNCTION(Server, Reliable)
	void Server_SubmitQTEInput();

	UPROPERTY()
	TObjectPtr<UQTEWidget> QTEWidgetInstance;

	// ==========================================
	// [�ű�] Stage 2 ���� QTE (A/D ��Ÿ)
	// ==========================================
	UPROPERTY(EditDefaultsOnly, Category = "UI|IndividualQTE")
	TSubclassOf<UADQTEWidget> ADQTEWidgetClass;

	UPROPERTY()
	TObjectPtr<UADQTEWidget> ADQTEWidgetInstance;

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

	//DeathCount UI
	UPROPERTY(EditDefaultsOnly, Category = "UI/DeathCount")
	TSubclassOf<class UUserWidget> RespawnCountWidgetClass;
	UPROPERTY()
	class URespawnCountWidget* RespawnCountWidget;

	UFUNCTION()
	void UpdateUI_RespawnCount(int32 CurrentLives);
	
	UPROPERTY(EditDefaultsOnly, Category = "UI|GroupQTE")
	TSubclassOf<UGQTEWidget> GQTEWidgetClass;

	UPROPERTY()
	TObjectPtr<UGQTEWidget> GQTEWidgetInstance;
};