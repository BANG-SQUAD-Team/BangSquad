#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WebPad.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UCLASS()
class PROJECT_BANG_SQUAD_API AWebPad : public AActor
{
    GENERATED_BODY()

public:
    AWebPad();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** 영역 감지용 박스 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* CollisionBox;

    /** 바닥 거미줄 데칼 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* WebMeshComp;

    /** 감속 비율 (0.3f면 원래 속도의 30%로 감소) */
    UPROPERTY(EditAnywhere, Category = "Balance")
    float SlowPercentage = 0.7f;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    /** 현재 밟고 있는 대상 관리용 명단 */
    UPROPERTY()
    TArray<class ABaseCharacter*> AffectedPlayers;

    UPROPERTY()
    TMap<class ACharacter*, float> EnemyOriginalSpeeds;
};