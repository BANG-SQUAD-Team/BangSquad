#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Components/ArrowComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TitanCharacter.generated.h"

class ATitanRock;
class AAIController;

UCLASS()
class PROJECT_BANG_SQUAD_API ATitanCharacter : public ABaseCharacter
{
    GENERATED_BODY()

public:
    ATitanCharacter();

protected:
    // =================================================================
    // [생명주기 및 오버라이드]
    // =================================================================
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void OnDeath() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // =================================================================
    // [입력 핸들러]
    // =================================================================
    virtual void Attack() override;      // 평타 (A/B 콤보)
    virtual void JobAbility() override;  // 잡기 / 던지기
    virtual void Skill1() override;      // 바위 생성 및 던지기
    virtual void Skill2() override;      // 돌진 (Charge)
    
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_Skill1();
    
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_Skill2();

public:
    // 블루프린트 및 애님 노티파이 연동 함수
    UFUNCTION(BlueprintCallable)
    void ExecuteGrab();

    UFUNCTION(BlueprintCallable, Category = "Titan|Skill")
    void ExecuteSpawnRock();

    UFUNCTION(BlueprintCallable, Category = "Titan|Skill")
    void ExecuteThrowRock();

protected:
    // =================================================================
    // [카메라 연출 설정]
    // =================================================================
    
    // 조준(잡기) 상태일 때의 카메라 오프셋
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Aiming")
    FVector AimingSocketOffset = FVector(0.0f, 70.0f, 30.0f); 

    // 기본 상태 카메라 오프셋 (BeginPlay시 저장)
    FVector DefaultSocketOffset;

    // 카메라 전환 속도
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Aiming")
    float CameraInterpSpeed = 5.0f;
    
    // 궤적 계산 및 시각화 업데이트
    void UpdateTrajectory();

    // 궤적 표시 여부 설정
    void ShowTrajectory(bool bShow);

    // =================================================================
    // [궤적(Trajectory) 시스템]
    // =================================================================
    
    // 궤적 경로 데이터
    UPROPERTY(VisibleAnywhere, Category = "Trajectory")
    USplineComponent* TrajectorySpline;

    // 궤적 시각화용 메쉬 풀 (Pooling)
    UPROPERTY()
    TArray<USplineMeshComponent*> SplineMeshes;

    // 궤적에 사용할 메쉬 에셋
    UPROPERTY(EditDefaultsOnly, Category = "Trajectory")
    UStaticMesh* TrajectoryMesh;

    // 궤적 메쉬 재질
    UPROPERTY(EditDefaultsOnly, Category = "Trajectory")
    UMaterialInterface* TrajectoryMaterial;
    
    // 궤적 각도 보정값 (Z축)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
    float TrajectoryZBias = 0.0f; 

    // 궤적 계산용 중력 배율
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
    float TrajectoryGravityScale = 1.0f;
    
    // 기본 던지기 힘
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
    float ThrowForce = 800.0f;
    
    // 캐릭터 던지기 시 힘 보정 배율 (캐릭터 저항 보정용)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
    float ThrowCharacterMultiplier = 1.3f;
    
    // =================================================================
    // [전투 및 판정 (Combat & Trace)]
    // =================================================================

    // 공격 판정 박스 크기
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    FVector HitBoxSize = FVector(80.0f, 80.0f, 80.0f);

    // 판정 지속 시간
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    float HitDuration = 0.25f;

    FTimerHandle AttackHitTimerHandle; 
    FTimerHandle HitLoopTimerHandle;   

    FVector LastHandLocation;

    UPROPERTY()
    TSet<AActor*> SwingDamagedActors; // 중복 타격 방지용 셋

    // 근접 공격 판정 함수들
    void StartMeleeTrace();
    void PerformMeleeTrace();
    void StopMeleeTrace();

    FName MyAttackSocket;

    // 클라이언트 캐릭터 위치/상태 강제 동기화
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_FixMesh(ACharacter* Victim);

    // =================================================================
    // [네트워크: 기본 공격]
    // =================================================================
    UFUNCTION(Server, Reliable)
    void Server_Attack(FName SkillName);

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_Attack(FName SkillName);

    // =================================================================
    // [네트워크: 직업 스킬 (잡기/던지기)]
    // =================================================================
    UFUNCTION(Server, Reliable)
    void Server_TryGrab(AActor* TargetToGrab);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UArrowComponent* ThrowSpawnPoint;

    UFUNCTION(Server, Reliable)
    void Server_ThrowTarget(FVector ThrowStartLocation);

    void AutoThrowTimeout();

    // 잡힌 상태 동기화 (RepNotify)
    UFUNCTION()
    void OnRep_GrabbedActor();

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayJobMontage(FName SectionName);

    // 광역 충돌 처리 (Throw Impact)
    void PerformRadialImpact(FVector Origin, float Radius, float Damage, float RadialForce, AActor* IgnoreTarget = nullptr);

    // 던져진 액터가 충돌했을 때 콜백
    UFUNCTION()
    void OnThrownActorHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);
    
    // 던져진 액터의 물리 상태 정리 (멀티캐스트)
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_ForceThrowCleanup(AActor* TargetActor, FVector Velocity);
    
    // =================================================================
    // [네트워크: 스킬 1 (바위 던지기)]
    // =================================================================
    UFUNCTION(Server, Reliable)
    void Server_Skill1();

    void ThrowRock();

    UFUNCTION(Server, Reliable)
    void Server_SpawnRock();

    UFUNCTION(Server, Reliable)
    void Server_ThrowRock();

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_SpawnRockEffects(FVector SpawnLocation);

    // =================================================================
    // [네트워크: 스킬 2 (돌진)]
    // =================================================================
    UFUNCTION(Server, Reliable)
    void Server_Skill2();

    UFUNCTION()
    void OnChargeOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnChargeHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

    UPROPERTY(EditDefaultsOnly, Category = "Skill|FX")
    class UNiagaraSystem* DashWindEffect; // UParticleSystem -> UNiagaraSystem 변경

    // 2. 실제 몸에 붙을 나이아가라 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|FX")
    class UNiagaraComponent* DashPSC;

    // 3. 함수는 그대로 유지
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_ToggleDashFX(bool bActive);

    // =================================================================
    // [헬퍼 함수 및 내부 로직]
    // =================================================================
    void ProcessSkill(FName SkillRowName, FName StartSectionName = NAME_None);
    void UpdateHoverHighlight();
    void SetHighlight(AActor* Target, bool bEnable);
    void SetHeldState(ACharacter* Target, bool bIsHeld);

    // 캐릭터 상태 복구 (던져진 후 기상)
    UFUNCTION()
    void RecoverCharacter(ACharacter* Victim);

    void StopCharge();
    void ResetCooldown();
    void ResetSkill1Cooldown();
    void ResetSkill2Cooldown();

private:
    // 공격 콤보 관리
    bool bIsNextAttackA = true;
    float AttackCooldownTime = 0.f;

    // 잡기 대상 및 상태
    UPROPERTY(ReplicatedUsing = OnRep_GrabbedActor)
    AActor* GrabbedActor = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    bool bIsGrabbing = false;

    UPROPERTY()
    AActor* HoveredActor = nullptr;

    FTimerHandle GrabTimerHandle;
    FTimerHandle CooldownTimerHandle;

    FTimerHandle MeleeStopTimerHandle;
    
    float GrabMaxDuration = 5.0f;
    float ThrowCooldownTime = 3.0f;

    // 스킬 1 (바위) 데이터
    UPROPERTY(EditDefaultsOnly, Category = "Skill|Rock")
    TSubclassOf<ATitanRock> RockClass;

    UPROPERTY(EditDefaultsOnly, Category = "Skill|FX")
    UMaterialInterface* GroundCrackDecal;

    UPROPERTY()
    ATitanRock* HeldRock = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Skill|Rock")
    float RockThrowForce = 2500.0f;

    FTimerHandle RockThrowTimerHandle;
    FTimerHandle Skill1CooldownTimerHandle;
    float Skill1CooldownTime = 3.0f;
    bool bIsSkill1Cooldown = false;

    // 스킬 2 (돌진) 데이터
    FTimerHandle Skill2CooldownTimerHandle;
    FTimerHandle ChargeTimerHandle;
    float Skill2CooldownTime = 5.0f;
    bool bIsSkill2Cooldown = false;
    bool bIsCharging = false;

    UPROPERTY()
    TArray<AActor*> HitVictims;

    // 공통 상태
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    bool bIsCooldown = false;

    float CurrentSkillDamage = 0.0f;
    float DefaultGroundFriction;
    float DefaultGravityScale;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data", meta = (AllowPrivateAccess = "true"))
    class UDataTable* SkillDataTable;
};