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
	virtual void Attack() override;      // 평타
	virtual void JobAbility() override;  // 잡기/던지기
	virtual void Skill1() override;      // 바위 던지기
	virtual void Skill2() override;      // 돌진
	
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Skill1();
	
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Skill2();

public:
	// 잡기 실행 함수 (몽타주 등에서 호출 가능)
	UFUNCTION(BlueprintCallable)
	void ExecuteGrab();

	UFUNCTION(BlueprintCallable, Category = "Titan|Skill")
	void ExecuteSpawnRock();

	UFUNCTION(BlueprintCallable, Category = "Titan|Skill")
	void ExecuteThrowRock();

protected:
	
	// =================================================================
	// [카메라 연출 변수]
	// =================================================================
    
	// 잡기 모드일 때 목표 카메라 오프셋 (Y값을 양수로 하면 캐릭터가 왼쪽으로 감)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Aiming")
	FVector AimingSocketOffset = FVector(0.0f, 70.0f, 30.0f); 

	// 평소 카메라 오프셋 (시작할 때 자동 저장됨)
	FVector DefaultSocketOffset;

	// 카메라 이동 속도 (높을수록 빠름)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Aiming")
	float CameraInterpSpeed = 5.0f;
	
	// 궤적 계산 및 시각화 업데이트 함수
	void UpdateTrajectory();

	// 궤적을 끄고 켜는 함수
	void ShowTrajectory(bool bShow);

	// =================================================================
	// [궤적 시스템 변수]
	// =================================================================
    
	// 1. 궤적 경로 데이터를 담을 스플라인
	UPROPERTY(VisibleAnywhere, Category = "Trajectory")
	USplineComponent* TrajectorySpline;

	// 2. 점선(메쉬)들을 관리할 배열 (매 프레임 생성하면 느리므로 풀링 사용)
	UPROPERTY()
	TArray<USplineMeshComponent*> SplineMeshes;

	// 3. 궤적에 사용할 메쉬 (에디터에서 설정: 큐브나 원기둥)
	UPROPERTY(EditDefaultsOnly, Category = "Trajectory")
	UStaticMesh* TrajectoryMesh;

	// 4. 궤적 메쉬에 입힐 재질 (에디터에서 설정: 형광색 등)
	UPROPERTY(EditDefaultsOnly, Category = "Trajectory")
	UMaterialInterface* TrajectoryMaterial;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
	float TrajectoryZBias = 0.0f; // 기본값 0

	// 던지는 힘 (이것도 에디터에서 보게 설정)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
	float ThrowForce = 800.0f;
	
	// =================================================================
	// [공격 판정 (Trace/Sweep) 변수]
	// =================================================================

	// 공격 판정 박스 크기
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	FVector HitBoxSize = FVector(80.0f, 80.0f, 80.0f);

	// 판정 지속 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float HitDuration = 0.25f;

	// 타이머 핸들
	FTimerHandle AttackHitTimerHandle; // 딜레이용
	FTimerHandle HitLoopTimerHandle;   // 반복 판정용

	// 궤적 계산용
	FVector LastHandLocation;

	UPROPERTY()
	TSet<AActor*> SwingDamagedActors; // 중복 타격 방지

	// 판정 함수들
	void StartMeleeTrace();
	void PerformMeleeTrace();
	void StopMeleeTrace();

	FName MyAttackSocket;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_FixMesh(ACharacter* Victim);

	// =================================================================
	// [네트워크: 평타 (Attack)]
	// =================================================================
	UFUNCTION(Server, Reliable)
	void Server_Attack(FName SkillName);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Attack(FName SkillName);

	// =================================================================
	// [네트워크: 직업 스킬 (JobAbility - Grab/Throw)]
	// =================================================================
	UFUNCTION(Server, Reliable)
	void Server_TryGrab(AActor* TargetToGrab);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UArrowComponent* ThrowSpawnPoint;

	UFUNCTION(Server, Reliable)
	void Server_ThrowTarget(FVector ThrowStartLocation);

	void AutoThrowTimeout();

	UFUNCTION()
	void OnRep_GrabbedActor();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayJobMontage(FName SectionName);

	void PerformRadialImpact(FVector Origin, float Radius, float Damage, float RadialForce, AActor* IgnoreTarget = nullptr);

	// 던져진 대상이 어딘가 부딪혔을 때 호출될 함수 (델리게이트용)
	UFUNCTION()
	void OnThrownActorHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ForceThrowCleanup(AActor* TargetActor, FVector Velocity);
	
	// =================================================================
	// [네트워크: 스킬 1 (Rock Throw)]
	// =================================================================
	UFUNCTION(Server, Reliable)
	void Server_Skill1();

	void ThrowRock();

	UFUNCTION(Server, Reliable)
	void Server_SpawnRock();

	UFUNCTION(Server, Reliable)
	void Server_ThrowRock();

	// =================================================================
	// [네트워크: 스킬 2 (Charge)]
	// =================================================================
	UFUNCTION(Server, Reliable)
	void Server_Skill2();

	UFUNCTION()
	void OnChargeOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnChargeHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	// =================================================================
	// [헬퍼 함수 및 변수]
	// =================================================================
	void ProcessSkill(FName SkillRowName, FName StartSectionName = NAME_None);
	void UpdateHoverHighlight();
	void SetHighlight(AActor* Target, bool bEnable);
	void SetHeldState(ACharacter* Target, bool bIsHeld);

	UFUNCTION()
	void RecoverCharacter(ACharacter* Victim);

	void StopCharge();
	void ResetCooldown();
	void ResetSkill1Cooldown();
	void ResetSkill2Cooldown();

private:
	// 공격 관련
	bool bIsNextAttackA = true;
	float AttackCooldownTime = 0.f;

	// 잡기 관련
	UPROPERTY(ReplicatedUsing = OnRep_GrabbedActor)
	AActor* GrabbedActor = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsGrabbing = false;

	UPROPERTY()
	AActor* HoveredActor = nullptr;

	FTimerHandle GrabTimerHandle;
	FTimerHandle CooldownTimerHandle;
	
	float GrabMaxDuration = 5.0f;
	float ThrowCooldownTime = 3.0f;

	// 스킬 1 (바위) 관련
	UPROPERTY(EditDefaultsOnly, Category = "Skill|Rock")
	TSubclassOf<ATitanRock> RockClass;

	UPROPERTY()
	ATitanRock* HeldRock = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Skill|Rock")
	float RockThrowForce = 2500.0f;

	FTimerHandle RockThrowTimerHandle;
	FTimerHandle Skill1CooldownTimerHandle;
	float Skill1CooldownTime = 3.0f;
	bool bIsSkill1Cooldown = false;

	// 스킬 2 (돌진) 관련
	FTimerHandle Skill2CooldownTimerHandle;
	FTimerHandle ChargeTimerHandle;
	float Skill2CooldownTime = 5.0f;
	bool bIsSkill2Cooldown = false;
	bool bIsCharging = false;

	UPROPERTY()
	TArray<AActor*> HitVictims;

	// 공통
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsCooldown = false;

	float CurrentSkillDamage = 0.0f;
	float DefaultGroundFriction;
	float DefaultGravityScale;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data", meta = (AllowPrivateAccess = "true"))
	class UDataTable* SkillDataTable;
};