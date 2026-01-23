#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Project_Bang_Squad/Game/InteractionInterface.h" // 경로 확인 필요
#include "BS_Switch.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API ABS_Switch : public AActor, public IInteractionInterface
{
	GENERATED_BODY()

public:
	ABS_Switch();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// 인터페이스 구현 (상호작용 시 호출됨)
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;

	// 네트워크 변수 복제 설정
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 변수가 변했을 때 클라이언트에서 실행될 함수 (Late-join 동기화용)
	UFUNCTION()
	void OnRep_IsActivated();

public:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UStaticMeshComponent* BaseMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UStaticMeshComponent* HandleMesh; // 실제로 움직일 메쉬

	UPROPERTY(EditInstanceOnly, Category = "Settings")
	class ABS_Door* TargetDoor; // 연결된 문

private:
	UPROPERTY(ReplicatedUsing = OnRep_IsActivated)
	bool bIsActivated = false;

	UPROPERTY(Replicated)
	float ActivationTime = -1.f;

	FVector InitialHandleLocation;
};