#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/TimelineComponent.h"
#include "BossPlatform.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API ABossPlatform : public AActor
{
	GENERATED_BODY()

public:
	ABossPlatform();
	virtual void BeginPlay() override;

	// 명령 함수 (서버에서 호출하면 클라이언트로 전파됨)
	void SetWarning(bool bActive);
	void MoveDown(bool bPermanent);
	void MoveUp();

	// 상태 확인
	bool IsWalkable() const { return !bIsDown && !bIsPermanentlyDown; }
	int32 GetPlayerCount() const { return OverlappingPlayers.Num(); }
	int32 PlatformID = -1;

	// [네트워크] 변수 동기화 필수 선언
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* MeshComp;

	UPROPERTY(VisibleAnywhere)
	class UBoxComponent* TriggerBox;

	UPROPERTY(VisibleAnywhere)
	UTimelineComponent* MoveTimeline;

	UPROPERTY(EditDefaultsOnly, Category = "Timeline")
	UCurveFloat* MovementCurve;

	UPROPERTY(EditAnywhere, Category = "Effect")
	float ShakeIntensity = 5.0f;

	UPROPERTY()
	UMaterialInstanceDynamic* DynMaterial;

	// [네트워크] 늦게 접속한 유저를 위한 상태 변수 동기화
	UPROPERTY(Replicated)
	bool bIsDown = false;

	UPROPERTY(Replicated)
	bool bIsPermanentlyDown = false;

	FVector InitialLocation;
	float DownDepth = -500.0f;

	TSet<AActor*> OverlappingPlayers;

	// [네트워크] 멀티캐스트 함수 선언 (모든 클라이언트 동시 실행)
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetWarning(bool bActive);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_MoveDown(bool bPermanent);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_MoveUp();

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void HandleProgress(float Value);

	UFUNCTION()
	void OnMoveFinished();
};