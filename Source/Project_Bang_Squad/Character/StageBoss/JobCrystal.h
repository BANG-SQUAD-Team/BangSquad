#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Project_Bang_Squad/Core/BSGameInstance.h" // EJobType 있음
#include "JobCrystal.generated.h"

class AStageBossBase;

UCLASS()
class PROJECT_BANG_SQUAD_API AJobCrystal : public AActor
{
    GENERATED_BODY()

public:
    AJobCrystal();

    // 이 수정을 파괴해야 하는 직업 (에디터에서 설정)
    UPROPERTY(EditInstanceOnly, Category = "Gimmick")
    EJobType RequiredJobType;

    // 연결된 보스 (파괴 시 알림용)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick")
    AStageBossBase* TargetBoss;

    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UStaticMeshComponent* MeshComp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxHealth = 100.0f;

    float CurrentHealth;
};