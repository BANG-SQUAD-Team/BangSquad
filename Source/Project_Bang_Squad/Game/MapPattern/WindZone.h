#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WindZone.generated.h"

class UBoxComponent;
class UNiagaraComponent;
class UArrowComponent;
class UUserWidget;

UCLASS()
class PROJECT_BANG_SQUAD_API AWindZone : public AActor
{
	GENERATED_BODY()
    
public: 
	AWindZone();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

	// 리플리케이션(동기화) 필수 함수
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UArrowComponent* ArrowComp;
    
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* WindBox;
    
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UNiagaraComponent* WindVFX;
    
	// ==========================================
	// 바람 사이클 설정
	// ==========================================
	UPROPERTY(EditAnywhere, Category = "Wind Cycle")
	float WindStrength = 2000.0f; 
    
	UPROPERTY(EditAnywhere, Category = "Wind Cycle")
	float GustDuration = 1.0f;
    
	UPROPERTY(EditAnywhere, Category = "Wind Cycle")
	float WarningDuration = 3.0f; 
	
	UPROPERTY(EditAnywhere, Category = "Wind Cycle")
	float CalmDuration = 5.0f;
    
	UPROPERTY(EditAnywhere, Category = "Wind Setting")
	bool bPushForward = true;
    
	UPROPERTY(EditAnywhere, Category = "Wind Setting")
	TEnumAsByte<ECollisionChannel> BlockChannel = ECC_Visibility;

	// ==========================================
	// UI 설정
	// ==========================================
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UUserWidget> WarningWidgetClass;
	
	UPROPERTY()
	UUserWidget* WarningWidgetInstance;
	
	// ==========================================
	// 네트워크 동기화 변수
	// ==========================================
    
	// 서버에서 이 값이 바뀌면, 클라이언트의 OnRep 함수가 자동으로 실행됩니다.
	UPROPERTY(ReplicatedUsing = OnRep_IsGusting)
	bool bIsGusting = false;
    
	// 경고 상태 여부
	UPROPERTY(ReplicatedUsing = OnRep_IsWarning)
	bool bIsWarning = false;
	
	UFUNCTION()
	void OnRep_IsGusting();
	
	UFUNCTION()
	void OnRep_IsWarning();

	FTimerHandle GustCycleTimer;

	void StartWarning();
	void StartGust();
	void StopGust();
	
	void UpdateWarningUI(bool bShow);
};