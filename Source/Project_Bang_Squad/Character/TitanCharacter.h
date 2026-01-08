#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "TitanCharacter.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API ATitanCharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
	ATitanCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// 기본 공격 및 스킬 오버라이드
	virtual void Attack() override;
	virtual void JobAbility() override;
	virtual void Skill1() override;
	virtual void Skill2() override; // 돌진 스킬

	// 데이터 테이블 기반 스킬 처리
	void ProcessSkill(FName SkillRowName);

	// 잡기 대상 하이라이트
	void UpdateHoverHighlight();

	// [충돌 감지] 돌진 중 적과 부딪혔을 때 처리
	UFUNCTION()
	void OnChargeHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

private:
	// ==========================================
	// 잡기(JobAbility) 관련 변수
	// ==========================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsGrabbing = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsCooldown = false;

	UPROPERTY()
	AActor* GrabbedActor = nullptr;

	UPROPERTY()
	AActor* HoveredActor = nullptr;

	FTimerHandle GrabTimerHandle;
	FTimerHandle CooldownTimerHandle;

	float GrabMaxDuration = 5.0f;
	float ThrowCooldownTime = 3.0f;

	// ==========================================
	// 돌진(Skill2) 관련 변수
	// ==========================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsCharging = false;

	// [추가] 돌진 전용 쿨타임 변수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsSkill2Cooldown = false;

	FTimerHandle ChargeTimerHandle;         // 돌진 지속시간용
	FTimerHandle Skill2CooldownTimerHandle; // 쿨타임 체크용

	// [추가] 이미 때린 적 기억 (다단히트 방지)
	UPROPERTY()
	TArray<AActor*> HitVictims;

	float Skill2CooldownTime = 5.0f; // 쿨타임 5초 (원하는 대로 수정)
	float DefaultGroundFriction;     // 원래 마찰력 저장할 변수
	float DefaultGravityScale;
	float CurrentSkillDamage = 0.0f; // 데미지 저장

	// 내부 함수
	void StopCharge();         // 돌진 멈춤 & 마찰력 복구
	void ResetSkill2Cooldown(); // 쿨타임 초기화

	UFUNCTION()
	void OnChargeOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// ==========================================
	// 데이터 테이블
	// ==========================================
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data", meta = (AllowPrivateAccess = "true"))
	class UDataTable* SkillDataTable;

	// ==========================================
	// 내부 함수들
	// ==========================================
	void TryGrab();
	void ThrowTarget();
	void ResetCooldown();
	void SetHighlight(AActor* Target, bool bEnable);
};