#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CenterStatueManager.generated.h"

class UStaticMeshComponent;
class AEnemySpawner;
class UCurveFloat;
class USceneComponent;
class UArrowComponent;
class AStage3PuzzleManager;

UCLASS()
class PROJECT_BANG_SQUAD_API ACenterStatueManager : public AActor
{
	GENERATED_BODY()

public:
	ACenterStatueManager();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

public:
	// =================================================================
	// 컴포넌트
	// =================================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* DefaultSceneRoot; // 고정된 루트

	// 넘어질 방향을 지정하는 화살표
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UArrowComponent* FallDirectionArrow;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* StatueMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* LeftFireMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* RightFireMesh;

	// =================================================================
	// 설정
	// =================================================================
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Link")
	AEnemySpawner* BossSpawner;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Link")
	AStage3PuzzleManager* PuzzleManager;

	// 넘어지는 동작 커브 (필수 할당)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	UCurveFloat* FallCurve;

	// =================================================================
	// 함수
	// =================================================================
	UFUNCTION(BlueprintCallable)
	void ActivateLeftGoblet();

	UFUNCTION(BlueprintCallable)
	void ActivateRightGoblet();

private:
	// 네트워크 변수
	UPROPERTY(ReplicatedUsing = OnRep_LeftActive)
	bool bLeftActive = false;

	UPROPERTY(ReplicatedUsing = OnRep_RightActive)
	bool bRightActive = false;

	UFUNCTION() void OnRep_LeftActive();
	UFUNCTION() void OnRep_RightActive();

	bool bPuzzleCompleted = false;
	void CheckPuzzleCompletion();

	// --- [낙하 로직] ---
	UFUNCTION() void OnBossDefeated();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_StartFallSequence();

	// 직접 시간 계산을 위한 변수들
	bool bIsFalling = false;       // 지금 넘어지는 중인가?
	float CurrentCurveTime = 0.0f; // 경과 시간
	float MaxCurveTime = 0.0f;     // 커브 총 길이

	void OnFallFinished();

	void StartDestroyTimer();

	// 액터는 살리고 메쉬만 지우는 함수
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_DestroyStatueMesh();

	FTimerHandle DestroyTimerHandle;

	// 회전 계산용
	FQuat StartQuat;
	FQuat EndQuat;
};