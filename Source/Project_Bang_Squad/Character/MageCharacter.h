#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Engine/DataTable.h" 
#include "MageCharacter.generated.h"

class UTimelineComponent;
class APillar; 
class UCurveFloat; 



UCLASS()
class PROJECT_BANG_SQUAD_API AMageCharacter : public ABaseCharacter
{
    GENERATED_BODY()

public:
    AMageCharacter();

    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Timeline")
    UTimelineComponent* CameraTimelineComp;

    UPROPERTY(EditAnywhere, Category = "Timeline")
    UCurveFloat* CameraCurve;
    
    // =========================================================
    // 네트워크 (RPC)
    // =========================================================
    // 서버: 투사체 생성
    UFUNCTION(Server, Reliable)
    void Server_SpawnProjectile(UClass* ProjectileClassToSpawn, FVector Location, FRotator Rotation);
    void Server_SpawnProjectile_Implementation(UClass* ProjectileClassToSpawn, FVector Location, FRotator Rotation);

    // 서버: 몽타주 재생 알림
    UFUNCTION(Server, Reliable)
    void Server_PlayMontage(UAnimMontage* MontageToPlay);
    void Server_PlayMontage_Implementation(UAnimMontage* MontageToPlay);
    
    // 클라: 몽타주 재생 실행
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayMontage(UAnimMontage* MontageToPlay);
    void Multicast_PlayMontage_Implementation(UAnimMontage* MontageToPlay); 

    // 서버: 기둥 넘어뜨리기
    UFUNCTION(Server, Reliable)
    void Server_TriggerPillarFall(APillar* TargetPillar);
    void Server_TriggerPillarFall_Implementation(APillar* TargetPillar);

protected:
    virtual void BeginPlay() override;
    
    // 공격 함수들
    virtual void Attack() override;
    virtual void Skill1() override;
    virtual void Skill2() override;
    
    // =========================================================
    // 일반 공격 콤보 애니메이션 (Combo)
    // =========================================================
    int32 CurrentComboIndex = 0;
    FTimerHandle ComboResetTimer;
    
    void ResetCombo();

    // =========================================================
    // 투사체 발사 타이밍 (Timing)
    // =========================================================
    // 발사 지연 타이머
    FTimerHandle ProjectileTimerHandle;
    
    // 타이머 도는 동안 "뭘 쏠지" 저장해두는 변수
    UPROPERTY()
    UClass* PendingProjectileClass;

    // 타이머 끝나면 실제 발사하는 함수
    void SpawnDelayedProjectile();

    // =========================================================
    // 직업 능력 (Job Ability)
    // =========================================================
    virtual void JobAbility() override;
    void EndJobAbility();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data")
    UDataTable* SkillDataTable;

private:
    UFUNCTION()
    void CameraTimelineProgress(float Alpha);

    UFUNCTION()
    void OnCameraTimelineFinished();

    float DefaultArmLength;
    FVector DefaultSocketOffset;
    
    void ProcessSkill(FName SkillRowName);
    
    void UpdatePillarInteraction();
    void LockOnPillar(float DeltaTime);
    
    UPROPERTY(EditAnywhere, Category = "Telekinesis")
    float TraceDistance = 5000.f;

    UPROPERTY()
    APillar* FocusedPillar;

    UPROPERTY()
    APillar* CurrentTargetPillar;

    bool bIsJobAbilityActive = false;
};