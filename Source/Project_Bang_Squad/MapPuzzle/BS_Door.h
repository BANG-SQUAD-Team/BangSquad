#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BS_Door.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API ABS_Door : public AActor
{
	GENERATED_BODY()

public:
	ABS_Door();
	void OpenDoor(); // 서버에서 호출할 함수

protected:
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* LeftDoorMesh;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* RightDoorMesh;

	// 왼쪽 문이 열릴 때 회전할 목표 각도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door | Settings")
	float LeftTargetAngle = 110.f;

	// 오른쪽 문이 열릴 때 회전할 목표 각도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door | Settings")
	float RightTargetAngle = -110.f;

	// 문이 열리는 데 걸리는 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door | Settings")
	float OpenDuration = 1.5f;

private:
	UPROPERTY(Replicated)
	bool bIsOpen = false;

	UPROPERTY(Replicated)
	float OpenStartTime = -1.f;
};