#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AQTE_Trap.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API AQTE_Trap : public AActor
{
    GENERATED_BODY()

public:
    AQTE_Trap();

    // 서버에서 호출됨
    void InitializeTrap(class ACharacter* TargetPlayer, int32 RequiredMashCount);

    // 컨트롤러(서버)에서 연타 입력이 들어올 때 호출
    void AddQTEProgress();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UStaticMeshComponent* MeshComp;

private:
    UPROPERTY()
    class ACharacter* TrappedPlayer;

    UPROPERTY()
    class AStageBossPlayerController* TrappedPC; // 본인의 컨트롤러 클래스명에 맞게 수정

    int32 CurrentCount = 0;
    int32 TargetCount = 0;

    void BreakTrap();
};