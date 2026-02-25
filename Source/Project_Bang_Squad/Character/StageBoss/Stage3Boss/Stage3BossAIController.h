#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Stage3BossAIController.generated.h"

UENUM()
enum class EBoss3Skill : uint8 { Basic, Laser, Meteor, Break, Chase };

UCLASS()
class PROJECT_BANG_SQUAD_API AStage3BossAIController : public AAIController
{
	GENERATED_BODY()

protected:
	virtual void OnPossess(APawn* InPawn) override;
	void DecideNextAction();

	class AStage3Boss* Boss;
	AActor* TargetPlayer;
	FTimerHandle ActionTimer;

	float CombatStartTime = 0.0f;
	float LastTime_Laser = -100;
	float LastTime_Meteor = -100;
	float LastTime_Break = -100;

	const float CD_Laser = 10.0f;
	const float CD_Meteor = 20.0f;
	const float CD_Break = 15.0f;
};