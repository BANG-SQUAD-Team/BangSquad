// Source/Project_Bang_Squad/Character/Enemy/Stage2MidBoss.h

#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBossData.h"
#include "Stage2MidBoss.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API AStage2MidBoss : public AEnemyCharacterBase
{
	GENERATED_BODY()

public:
	AStage2MidBoss();

	// 애니메이션 노티파이에서 호출할 함수들
	UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
	void OnNotify_MeleeHitCheck(); // 지팡이 타격 시점

	UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
	void OnNotify_FireMagic();     // 마법 발사 시점

protected:
	virtual void BeginPlay() override;

public:
	// --- [AI가 호출하는 명령 함수들] ---

	// 1. 원거리 마법 발사 (리턴값: 몽타주 길이)
	float Execute_RangedAttack(AActor* Target);

	// 2. 타겟 근처로 텔레포트
	bool Execute_TeleportToTarget(AActor* Target);

	// 3. 근접 공격 (Sphere Trace)
	float Execute_MeleeAttack();

	// 데이터 에셋 접근
	FORCEINLINE UEnemyBossData* GetBossData() const { return BossData; }

protected:
	// --- [네트워크 비주얼 동기화] ---
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayMontage(UAnimMontage* Montage);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_TeleportFX(FVector Location);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Data")
	TObjectPtr<UEnemyBossData> BossData;
};