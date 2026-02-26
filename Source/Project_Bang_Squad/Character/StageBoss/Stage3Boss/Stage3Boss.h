// Source/Project_Bang_Squad/Character/StageBoss/Stage3Boss.h

#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/StageBoss/StageBossBase.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBossData.h"
#include "Stage3Boss.generated.h"

class APlatformManager;
class ABossPlatform;

/**
 * Stage 3 Boss: Molten Predator
 * - Gimmicks: 4x4 Platform, Laser, Meteor, Platform Break
 */
UCLASS()
class PROJECT_BANG_SQUAD_API AStage3Boss : public AStageBossBase
{
    GENERATED_BODY()

public:
    AStage3Boss();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // [Server-Only] 데미지 및 무적 처리
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

    // 체력 변경 델리게이트
    UFUNCTION()
    void OnHealthChanged(float CurrentHealth, float MaxHealth);

public:
    // --- Skills (AI 호출용) ---
    UFUNCTION(BlueprintCallable) float Execute_BasicAttack();
    UFUNCTION(BlueprintCallable) float Execute_Laser();
    UFUNCTION(BlueprintCallable) float Execute_Meteor();
    UFUNCTION(BlueprintCallable) float Execute_PlatformBreak();
    UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
    void DoMeleeHitCheck();


    // 레이저 타겟팅 헬퍼
    UFUNCTION(BlueprintCallable) void FindNearestPlayer();

    // [Getter]
    UEnemyBossData* GetBossData() const { return BossData; }

    // ==============================================================================
    // [UI] 보스 체력바 및 자막 시스템 (Stage 1 스타일)
    // ==============================================================================
    UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = "BS|UI")
    void Multicast_ShowBossSubtitle(const FText& Message, float Duration = 3.0f);

    UPROPERTY(EditDefaultsOnly, Category = "BS|UI")
    TSubclassOf<class UUserWidget> BossSubtitleWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "BS|UI")
    TSubclassOf<class UUserWidget> BossHPWidgetClass;

    UPROPERTY()
    class UUserWidget* ActiveBossHPWidget;

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_ShowBossHP(float MaxHP);

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_UpdateBossHP(float CurrentHP, float MaxHP);

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_HideBossHP();

    // [추가] 애니메이션 몽타주 재생을 모든 클라이언트에게 동기화
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayBossMontage(UAnimMontage* MontageToPlay, FName SectionName = NAME_None);

    // [추가] 메테오 이펙트 재생을 모든 클라이언트에게 동기화
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_SpawnMeteorFX(FVector SpawnLocation);

protected:
    // [데이터] 보스 데이터 에셋 (Stage1Boss 처럼 직접 선언)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Data")
    TObjectPtr<UEnemyBossData> BossData;

    // 메테오 폭발 이펙트
    UPROPERTY(EditAnywhere, Category = "Boss|FX")
    class UParticleSystem* MeteorExplosionFX;

    UPROPERTY(EditInstanceOnly, Category = "Setup")
    APlatformManager* PlatformManager;

    // 레이저 상태 변수
    bool bIsFiringLaser = false;
    UPROPERTY() AActor* LaserTargetActor;
    FTimerHandle LaserDamageTimer;

    UFUNCTION() void ApplyLaserDamage();

    // 도미노 하강 재귀 함수
    UFUNCTION() void ProcessDominoDrop(const TArray<class ABossPlatform*>& Targets, int32 Index);
};