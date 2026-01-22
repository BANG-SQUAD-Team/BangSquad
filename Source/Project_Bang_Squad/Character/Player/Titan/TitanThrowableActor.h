#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TitanThrowableActor.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API ATitanThrowableActor : public AActor
{
	GENERATED_BODY()

public:
	ATitanThrowableActor();

protected:
	virtual void BeginPlay() override;

public:
	// 던져질 물체의 외형 (Static Mesh)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComp;

	// 던져졌을 때 추가 효과(예: 날아가는 소리, 파티클)를 위해 열어둔 함수
	UFUNCTION(BlueprintNativeEvent, Category = "Titan|Throw")
	void OnThrown(FVector Direction);
};