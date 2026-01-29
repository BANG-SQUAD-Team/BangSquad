#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SeaMonster.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API ASeaMonster : public AActor
{
	GENERATED_BODY()

public:
	ASeaMonster();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Appearance")
	class USkeletalMeshComponent* MeshComp;

	/** 에디터에서 드래그해서 위치를 잡는 발사 지점 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Monster|Combat")
	class USceneComponent* ProjectileSpawnPoint;

	/** 던질 공 블루프린트 클래스 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Combat")
	TSubclassOf<class ASeaMonsterProjectile> ProjectileClass;

	/** 공을 던지는 주기 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Combat")
	float FireInterval = 2.5f;

	/** [핵심 1] 날아갈 거리 (언리얼 단위 cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Combat")
	float FireDistance = 1200.0f;

	/** [핵심 2] 발사 각도/곡선 정도 (0.1: 직선에 가까움, 0.5: 중간, 1.0: 높은 포물선) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Combat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LaunchArc = 0.2f;

	FTimerHandle TimerHandle_Attack;
	void Fire();

public:
	virtual void Tick(float DeltaSeconds) override;
};