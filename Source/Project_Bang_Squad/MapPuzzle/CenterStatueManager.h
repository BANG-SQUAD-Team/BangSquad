#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/TimelineComponent.h"
#include "CenterStatueManager.generated.h"

class UStaticMeshComponent;
class AEnemySpawner;
class UCurveFloat;
class USceneComponent;
class UArrowComponent; // [추가] 화살표 컴포넌트

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
	// 컴포넌트 (계층 구조가 핵심)
	// =================================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* DefaultSceneRoot; // 부동의 루트

	// [추가] 에디터에서 넘어질 방향을 지정하는 화살표
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UArrowComponent* FallDirectionArrow;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* StatueMesh; // 넘어질 놈

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* LeftFireMesh; // 안 넘어질 놈 1

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* RightFireMesh; // 안 넘어질 놈 2

	// =================================================================
	// 설정
	// =================================================================
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Link")
	AEnemySpawner* BossSpawner;

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
	UPROPERTY(ReplicatedUsing = OnRep_LeftActive)
	bool bLeftActive = false;

	UPROPERTY(ReplicatedUsing = OnRep_RightActive)
	bool bRightActive = false;

	UFUNCTION() void OnRep_LeftActive();
	UFUNCTION() void OnRep_RightActive();

	bool bPuzzleCompleted = false;
	void CheckPuzzleCompletion();

	// --- [보스 처치 및 낙하 로직] ---
	UFUNCTION() void OnBossDefeated();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_StartFallSequence();

	UFUNCTION()
	void HandleFallProgress(float Value);

	UFUNCTION()
	void OnFallFinished();
	void DestroyStatue();
	FTimerHandle DestroyTimerHandle;

	bool bIsFalling = false;       // 지금 넘어지는 중인가?
	float CurrentCurveTime = 0.0f; // 경과 시간 (0초 -> 2초)
	float MaxCurveTime = 0.0f;     // 커브의 총 길이 (끝나는 시간)

	// 회전 계산용 쿼터니언
	FQuat StartQuat;
	FQuat EndQuat;
};