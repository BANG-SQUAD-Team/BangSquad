#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "PaladinCharacter.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API APaladinCharacter : public ABaseCharacter
{
    GENERATED_BODY()
    
public:
    APaladinCharacter();
    
protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    
    virtual void Attack() override;

    // =============================================================
    // [공격 판정 관련 변수 및 함수]
    // =============================================================
protected:
    // 스킬 데미지 캐싱용
    float CurrentSkillDamage = 0.0f;
    
    // 망치 머리 크기 (절반 크기 Extent 입력, 예: 30, 30, 30)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    FVector HammerHitBoxSize = FVector(30.0f, 30.0f, 30.0f);                       
    
    // 공격 시작 딜레이 타이머
    FTimerHandle AttackHitTimerHandle;

    // [추가] 궤적 판정 반복 타이머
    FTimerHandle HitLoopTimerHandle;

    // [추가] 공격 판정 유지 시간 (망치를 휘두르는 시간, 예: 0.2~0.3초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    float HitDuration = 0.25f; 

    // [추가] 이전 프레임의 망치 위치 (궤적 연결용)
    FVector LastHammerLocation;

    // [추가] 한 번 휘두를 때 이미 맞은 적 목록 (중복 타격 방지)
    TSet<AActor*> SwingDamagedActors;

    // [변경] 단발성 ExecuteMeleeHit 삭제 -> 3단계 함수로 분리
    void StartMeleeTrace();   // 판정 시작 (딜레이 직후 호출)
    void PerformMeleeTrace(); // 매 프레임 궤적 검사
    void StopMeleeTrace();    // 판정 종료

    // =============================================================
    // [콤보 및 데이터 테이블]
    // =============================================================
    int32 CurrentComboIndex = 0;
    FTimerHandle ComboResetTimer;
    void ResetCombo();
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data")
    UDataTable* SkillDataTable;
    
    void ProcessSkill(FName SkillRowName);
    
    // =============================================================
    // [네트워크 RPC]
    // =============================================================
public:
    UFUNCTION(Server, Reliable)
    void Server_PlayMontage(UAnimMontage* MontageToPlay);
    void Server_PlayMontage_Implementation(UAnimMontage* MontageToPlay);
    
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayMontage(UAnimMontage* MontageToPlay);
    void Multicast_PlayMontage_Implementation(UAnimMontage* MontageToPlay);
};