
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IcePad.generated.h"

class UBoxComponent;
class UDecalComponent;

UCLASS()
class PROJECT_BANG_SQUAD_API AIcePad : public AActor
{
	GENERATED_BODY()
	
public:	
	AIcePad();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	class UMaterialInterface* FrostOverlayMaterial;
	
	// 서버가 모든 클라이언트에게 오버레이 머티리얼을 씌우라고 명령하는 함수
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetOverlayMaterial(class ACharacter* TargetCharacter, class UMaterialInterface* OverlayMat);
	
	//  장판 파괴 타이머 핸들
	FTimerHandle DestroyTimerHandle;

	//  방송을 먼저 쏘고 파괴할 함수
	UFUNCTION()
	void DeactivateAndDestroy();
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_HidePad();
	
	// 영역 감지용 박스
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* CollisionBox;
	
	// 바닥에 그려질 얼음 문양
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UDecalComponent* IceDecalComp;
	
	// 감속 비율 
	UPROPERTY(EditAnywhere, Category = "Balance")
	float SlowPercentage = 0.3f;
	
	// 장판 지속 시간
	UPROPERTY(EditAnywhere, Category = "Balance")
	float Duration = 5.0f;
	
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
	
	// 플레이어 명단
	UPROPERTY()
	TArray<class ABaseCharacter*> AffectedPlayers;

	// 몬스터 명단과 '원래 속도'를 적어두는 장부
	UPROPERTY()
	TMap<class ACharacter*, float> EnemyOriginalSpeeds;
	
	// 1초마다 들어갈 장판 도트 데미지
	UPROPERTY(EditAnywhere, Category = "Balance")
	float DoTDamage = 5.0f;

	// 도트 딜용 타이머
	FTimerHandle DoTTimerHandle;

	// 도트 딜 실행 함수
	UFUNCTION()
	void ApplyDoTDamage();
	
};
