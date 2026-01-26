#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BS_Door.generated.h"

UENUM(BlueprintType)
enum class EDoorAction : uint8
{
	Open      UMETA(DisplayName = "Permanent Open"), // 완전 열림
	Close     UMETA(DisplayName = "Force Close"),    // 강제 닫힘
	TempOpen  UMETA(DisplayName = "Temporary Open")  // 일시적 열림
};

UCLASS()
class PROJECT_BANG_SQUAD_API ABS_Door : public AActor
{
	GENERATED_BODY()
public:
	ABS_Door();
	void ExecuteAction(EDoorAction Action); // 외부에서 명령을 내리는 창구

protected:
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* LeftDoorMesh;
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* RightDoorMesh;

	UPROPERTY(EditAnywhere, Category = "Settings") float LeftTargetAngle = 110.f;
	UPROPERTY(EditAnywhere, Category = "Settings") float RightTargetAngle = -110.f;
	UPROPERTY(EditAnywhere, Category = "Settings") float OpenDuration = 1.5f;

private:
	UPROPERTY(Replicated) bool bMasterOpen = false; // 스위치 등으로 영구 개방
	UPROPERTY(Replicated) bool bTempOpen = false;   // 버튼 등으로 일시적 개방
	float CurrentAlpha = 0.f;
};