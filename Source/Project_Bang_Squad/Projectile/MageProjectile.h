#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "MageProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UParticleSystem;
class UParticleSystemComponent;

UCLASS()
class PROJECT_BANG_SQUAD_API AMageProjectile : public AActor
{
    GENERATED_BODY()
    
public: 
    // ====================================================================================
    // [Section 1] 생성자 및 기본 설정
    // ====================================================================================
    AMageProjectile();
    
    // ====================================================================================
    // [Section 2] 투사체 스탯 및 데이터
    // ====================================================================================
    /** 캐릭터(Mage)가 생성할 때 전달해 줄 데미지 값 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Damage")
    float Damage = 10.0f; 
    
    /** 대상에 적중했을 때 폭발할 나이아가라 이펙트 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|VFX")
    UNiagaraSystem* FireImpactVFX;

protected:
    // ====================================================================================
    // [Section 3] 라이프사이클 이벤트
    // ====================================================================================
    virtual void BeginPlay() override;

    // ====================================================================================
    // [Section 4] 핵심 컴포넌트
    // ====================================================================================
    /** 투사체의 충돌 범위 (Root Component) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* SphereComp;
    
    /** 투사체의 외형 (에디터에서 비워두면 투명한 상태로 날아감) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* MeshComp;

    /** 투사체가 날아갈 때 보여줄 나이아가라 궤적/이펙트 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UNiagaraComponent* NiagaraComp;

    /** 투사체의 이동 속도와 중력을 제어하는 컴포넌트 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UProjectileMovementComponent* ProjectileMovement;

    // ====================================================================================
    // [Section 5] 충돌 처리 함수
    // ====================================================================================
    /** 캐릭터(Pawn), 몬스터 등을 뚫고 지나갈 때 (Overlap) 호출되는 함수 */
    UFUNCTION()
    virtual void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
       UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
       bool bFromSweep, const FHitResult& SweepResult);
    
    /** 벽, 바닥 등 단단한 물체에 막혔을 때 (Block) 호출되는 함수 */
    virtual void NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp,
       bool bSelfMoved, FVector HitLocation, 
       FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

    // ====================================================================================
    // [Section 6] 네트워크 (RPC)
    // ====================================================================================
    /** * 적중 시 모든 클라이언트 화면에서 폭발 이펙트를 재생하고, 
     * 투사체를 '가짜 죽음' 상태(가리기)로 만드는 함수 
     */
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_SpawnHitVFX(FVector Location, FRotator Rotation);
};