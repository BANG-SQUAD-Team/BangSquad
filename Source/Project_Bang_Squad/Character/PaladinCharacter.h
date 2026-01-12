#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "PaladinCharacter.generated.h"

/**
 * 
 */
UCLASS()
class PROJECT_BANG_SQUAD_API APaladinCharacter : public ABaseCharacter
{
	GENERATED_BODY()
	
public:
	APaladinCharacter();
	
protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	
	// 공격 (Attack)
	FTimerHandle AttackHitTimerHandle;
	
	void ExecuteMeleeHit();
	
	virtual void Attack() override;
	float CurrentSkillDamage = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackRange = 200.0f;
	
	// 공격 좌우 범위 (망치가 휩쓰는 너비)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackWidth = 150.0f;

	// 공격 높이 (위아래)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackHeight = 150.0f;
	
	// 콤보 카운트 관리
	int32 CurrentComboIndex = 0;
	FTimerHandle ComboResetTimer;
	
	// 콤보 초기화 함수
	void ResetCombo();
	
	// 스킬 데이터 처리 (데이터 테이블&딜레이)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data")
	UDataTable* SkillDataTable;
	
	// 실제 스킬 로직 실행
	void ProcessSkill(FName SkillRowName);
	
	// 네트워크 (RPC) - 애니메이션 동기화용
public:
	// 서버: 몽타주 재생 알림
	UFUNCTION(Server, Reliable)
	void Server_PlayMontage(UAnimMontage* MontageToPlay);
	void Server_PlayMontage_Implementation(UAnimMontage* MontageToPlay);
	
	// 클라이언트: 몽타주 재생 실행
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayMontage(UAnimMontage* MontageToPlay);
	void Multicast_PlayMontage_Implementation(UAnimMontage* MontageToPlay);
};
