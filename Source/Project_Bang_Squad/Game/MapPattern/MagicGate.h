#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MagicGate.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API AMagicGate : public AActor
{
	GENERATED_BODY()
    
public: 
	AMagicGate();
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override; // 필수 추가
    
	UFUNCTION(BlueprintCallable)
	void OpenGate();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UStaticMeshComponent* MeshComp;
    
	// 🔥변수 값이 바뀌면 모든 클라이언트에게 알림 (RepNotify)
	UPROPERTY(ReplicatedUsing = OnRep_IsOpen) 
	bool bIsOpen = false;

	// ] bIsOpen이 바뀌면 자동으로 호출되는 함수
	UFUNCTION()
	void OnRep_IsOpen();
    
	float TargetZ;
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate Setting")
	float OpenDistance = 400.0f;
};