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
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components")
	UStaticMeshComponent* MeshComp;

	UFUNCTION(BlueprintNativeEvent, Category = "Titan|Throw")
	void OnThrown(FVector Direction);
};