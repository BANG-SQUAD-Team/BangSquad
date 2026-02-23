// Source/Project_Bang_Squad/Character/Enemy/Stage3MidBoss.h

#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBossData.h"
#include "Stage3MidBoss.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API AStage3MidBoss : public AEnemyCharacterBase
{
	GENERATED_BODY()

public:
	AStage3MidBoss();

protected:
	virtual void BeginPlay() override;

public:
	// ==========================================================
	// [1] 애니메이션 노티파이 (몽타주에서 타격/발사 시점에 호출)
	// ==========================================================
	UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
	void OnNotify_MeleeHitCheck(); // 근접 평타 판정

	UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
	void OnNotify_FireRanged();    // 원거리 투사체 발사

	// ==========================================================
	// [2] AI 컨트롤러에서 호출할 명령 함수 (리턴: 몽타주 길이)
	// ==========================================================
	float Execute_BasicAttack();
	float Execute_RangedAttack(AActor* Target);

	FORCEINLINE UEnemyBossData* GetBossData() const { return BossData; }

protected:
	// ==========================================================
	// [3] 네트워크 동기화 및 실제 기능 구현부
	// ==========================================================
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayMontage(UAnimMontage* Montage);

	// 투사체 생성 (서버 전용)
	void FireProjectile(AActor* Target);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Data")
	TObjectPtr<UEnemyBossData> BossData;

	// [추가] 원거리 공격을 쏠 타겟을 잠시 기억해둘 변수
	UPROPERTY()
	TObjectPtr<AActor> CurrentTarget;
};