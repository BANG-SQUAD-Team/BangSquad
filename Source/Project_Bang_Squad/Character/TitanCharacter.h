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

	// [네트워크] 변수 복제(동기화) 설정을 위해 필수 오버라이드
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// 기본 공격 및 스킬 오버라이드
	virtual void Attack() override;
	virtual void JobAbility() override; // 클라이언트가 호출 -> 서버 요청
	virtual void Skill1() override;
	virtual void Skill2() override;     // 클라이언트가 호출 -> 서버 요청

	// 데이터 테이블 기반 스킬 처리
	void ProcessSkill(FName SkillRowName);

	// 잡기 대상 하이라이트 (클라이언트 전용 시각효과)
	void UpdateHoverHighlight();

	// [충돌 감지] 돌진 중 적과 부딪혔을 때 처리
	UFUNCTION()
	void OnChargeHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	// ==========================================
	// [네트워크] 서버 RPC 함수들 (Server_ 접두어)
	// ==========================================

	// 1. 잡기 요청 (서버야, 이 액터를 잡아줘)
	UFUNCTION(Server, Reliable)
	void Server_TryGrab(AActor* TargetToGrab);

	// 2. 던지기 요청 (서버야, 이 방향으로 던져줘)
	UFUNCTION(Server, Reliable)
	void Server_ThrowTarget(FVector ThrowDirection);

	// 3. 돌진 스킬 요청 (서버야, 돌진 시작해줘)
	UFUNCTION(Server, Reliable)
	void Server_Skill2();

private:
	// ==========================================
	// 잡기(JobAbility) 관련 변수
	// ==========================================

	// [중요] 서버에서 값이 바뀌면 클라도 알아야 함 (Replicated)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, meta = (AllowPrivateAccess = "true"))
	bool bIsGrabbing = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsCooldown = false;

	// [중요] 잡은 대상 동기화
	UPROPERTY(Replicated)
	AActor* GrabbedActor = nullptr;

	// 로컬 플레이어가 보고 있는 대상 (동기화 X, 내 화면용)
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsSkill2Cooldown = false;

	FTimerHandle ChargeTimerHandle;
	FTimerHandle Skill2CooldownTimerHandle;

	// 이미 때린 적 기억 (다단히트 방지)
	UPROPERTY()
	TArray<AActor*> HitVictims;

	float Skill2CooldownTime = 5.0f;
	float DefaultGroundFriction;
	float DefaultGravityScale;
	float CurrentSkillDamage = 0.0f;

	// 내부 함수
	void StopCharge();
	void ResetSkill2Cooldown();

	UFUNCTION()
	void OnChargeOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// ==========================================
	// 데이터 테이블
	// ==========================================
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data", meta = (AllowPrivateAccess = "true"))
	class UDataTable* SkillDataTable;

	// ==========================================
	// 내부 유틸리티
	// ==========================================
	void ResetCooldown();
	void SetHighlight(AActor* Target, bool bEnable);
};