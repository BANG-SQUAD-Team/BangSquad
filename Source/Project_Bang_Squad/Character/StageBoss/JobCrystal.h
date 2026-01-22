// Source/Project_Bang_Squad/Character/StageBoss/JobCrystal.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Project_Bang_Squad/Core/BSGameInstance.h"
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
    // [NEW] 게임 시작 시 스스로 보스를 찾기 위해 오버라이드
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UStaticMeshComponent* MeshComp;

    // [수정] 평타 2대에 파괴되도록 하기 위해 기본값을 2.0으로 설정합니다. (1대당 1씩 차감 예정)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gimmick")
    float MaxHealth = 2.0f;

    float CurrentHealth;
};