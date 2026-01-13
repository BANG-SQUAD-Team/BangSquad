#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "TitanCharacter.generated.h"

class AAIController;

UCLASS()
class PROJECT_BANG_SQUAD_API ATitanCharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
	ATitanCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// =============================================================
	// 입력 바인딩 함수 (Override)
	// =============================================================
	virtual void JobAbility() override; // 잡기/던지기
	virtual void Attack() override;     // 평타
	virtual void Skill1() override;     // 미구현 (예비)
	virtual void Skill2() override;     // 돌진 (Charge)

	// 스킬 공통 처리 (몽타주 재생 및 섹션 점프)
	void ProcessSkill(FName SkillRowName, FName StartSectionName = NAME_None);

	// 잡기 대상 하이라이트 갱신
	void UpdateHoverHighlight();

private:
	// =============================================================
	// [Job Ability] 잡기 & 던지기 관련 변수
	// =============================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsGrabbing = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsCooldown = false; // 잡기 쿨타임

	UPROPERTY(Replicated) // 멀티플레이 동기화
		AActor* GrabbedActor = nullptr;

	UPROPERTY()
	AActor* HoveredActor = nullptr;

	FTimerHandle GrabTimerHandle;
	FTimerHandle CooldownTimerHandle;

	float ThrowForce = 3500.f;
	float GrabMaxDuration = 5.0f;
	float ThrowCooldownTime = 3.0f;

	// 잡기 내부 로직
	void TryGrab();
	void ThrowTarget();
	void ResetCooldown();
	void SetHighlight(AActor* Target, bool bEnable);

	// 잡았다 풀려난 캐릭터 상태 복구
	UFUNCTION()
	void RecoverCharacter(ACharacter* Victim);

	// 잡혔을 때 AI/물리 끄기
	void SetHeldState(ACharacter* Target, bool bIsHeld);

public:
	// 애니메이션 노티파이에서 호출할 함수 (실제 부착 타이밍)
	UFUNCTION(BlueprintCallable)
	void ExecuteGrab();

protected:
	// 돌진 스킬 서버 실행 요청
	UFUNCTION(Server, Reliable)
	void Server_Skill2();

	// 돌진 충돌 감지 (Overlap)
	UFUNCTION()
	void OnChargeOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// 돌진 벽 충돌 감지 (Hit)
	UFUNCTION()
	void OnChargeHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void StopCharge();
	void ResetSkill2Cooldown();

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsCharging = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsSkill2Cooldown = false;

	// 돌진 중 중복 피격 방지용
	UPROPERTY()
	TArray<AActor*> HitVictims;

	FTimerHandle Skill2CooldownTimerHandle;
	FTimerHandle ChargeTimerHandle;

	// 밸런스 데이터
	float Skill2CooldownTime = 5.0f; // 쿨타임
	float CurrentSkillDamage = 0.0f; // 데이터테이블에서 가져옴

	// 물리 복구용 백업 변수
	float DefaultGroundFriction;
	float DefaultGravityScale;

	/** 타이탄 스킬 데이터 테이블 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data", meta = (AllowPrivateAccess = "true"))
	class UDataTable* SkillDataTable;
};