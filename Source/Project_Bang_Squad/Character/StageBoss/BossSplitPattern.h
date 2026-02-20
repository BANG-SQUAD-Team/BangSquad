#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossSplitPattern.generated.h"

class UTextRenderComponent;
class UBoxComponent;
class UNiagaraSystem;
//결과 통보용 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSplitPatternFinished, bool, bIsSuccess);

UCLASS()
class PROJECT_BANG_SQUAD_API ABossSplitPattern : public AActor
{
	GENERATED_BODY()

public:
	ABossSplitPattern();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;


public:
	// 보스쪽에서 바인딩할 이벤트
	UPROPERTY(BlueprintAssignable)
	FOnSplitPatternFinished OnSplitPatternFinished;

	// 패턴시작 ,보스로부터 패턴 유지 시간을 전달받도록 float 인자 추가!
	UFUNCTION(BlueprintCallable)
	void ActivatePattern(float PatternDuration);

protected:
	// A구역 Component
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* ZoneA_Mesh;

	UPROPERTY(VisibleAnywhere)
	UBoxComponent* ZoneA_Trigger;

	UPROPERTY(VisibleAnywhere)
	UTextRenderComponent* ZoneA_Text;

	// B구역 Component
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* ZoneB_Mesh;

	UPROPERTY(VisibleAnywhere)
	UBoxComponent* ZoneB_Trigger;

	UPROPERTY(VisibleAnywhere)
	UTextRenderComponent* ZoneB_Text;

	// [원칙 3] 리플리케이션 변수
	UPROPERTY(ReplicatedUsing = OnRep_UpdateTexts)
	int32 RequiredA;

	UPROPERTY(ReplicatedUsing = OnRep_UpdateTexts)
	int32 RequiredB;

	int32 CurrentA = 0;
	int32 CurrentB = 0;

	UFUNCTION()
	void OnRep_UpdateTexts();

	// 오버랩 이벤트
	UFUNCTION()
	void OnOverlapUpdate(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnEndOverlapUpdate(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void CheckResult();

	// [신규] 스폰 시 스스로 맵 바닥을 찾아 내려가는 함수
	void SnapToGround();

	// [원칙 2: 비주얼 동기화] 클라이언트 피드백용 멀티캐스트
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayResultEffect(bool bSuccess);

	// ==========================================
	// [이펙트 연출 변수] 블루프린트에서 할당
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TObjectPtr<UNiagaraSystem> SuccessVFX; // 반짝이는 이펙트

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TObjectPtr<UNiagaraSystem> FailVFX;    // 폭발하는 이펙트
};