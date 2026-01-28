#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WindZone.generated.h"

class UBoxComponent;
class UNiagaraComponent;
class UArrowComponent;
class UNiagaraSystem;

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

protected:
    // ==========================================
    // [기존 변수 유지]
    // ==========================================
    UPROPERTY(EditAnywhere, Category = "Wind Settings")
    float ShieldDamagePerSec = 5.0f;
    
    UPROPERTY(EditAnywhere, Category = "VFX")
    UNiagaraSystem* WindEffectTemplate;

    UPROPERTY(EditAnywhere, Category = "VFX")
    float SpawnInterval = 0.1f;

    void SpawnRandomWindVFX();
    FTimerHandle SpawnTimerHandle;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UArrowComponent* ArrowComp;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* WindBox;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UNiagaraComponent* WindVFX;

    // ==========================================
    // [부유 로직을 위해 이름 변경 및 추가된 변수]
    // ==========================================
    
    // 바람의 수평 세기 (뒤로 밀리는 힘)
    UPROPERTY(EditAnywhere, Category = "Wind Setting")
    float WindStrength = 500.0f; // 부유 로직에선 수치를 200~800 정도로 낮게 씁니다.

    // 캐릭터를 얼마나 높이 띄울지 (지면으로부터의 cm)
    UPROPERTY(EditAnywhere, Category = "Wind Setting")
    float MaxHoverHeight = 80.0f;

    // 위로 띄워 올리는 힘의 세기
    UPROPERTY(EditAnywhere, Category = "Wind Setting")
    float HoverForce = 1200.0f;

    // 바람에 노출되었을 때의 중력 배율 (0.0~0.5 추천, 낮을수록 둥실둥실함)
    UPROPERTY(EditAnywhere, Category = "Wind Setting")
    float WindGravityScale = 0.2f;

    UPROPERTY(EditAnywhere, Category = "Wind Setting")
    bool bPushForward = true;
    
    UPROPERTY(EditAnywhere, Category = "Wind Setting")
    TEnumAsByte<ECollisionChannel> BlockChannel = ECC_Visibility;

    // [삭제 추천]: GroundFrictionMultiplier는 이제 Falling 모드를 쓰므로 의미가 없어졌습니다.
};