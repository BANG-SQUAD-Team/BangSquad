// Source/Project_Bang_Squad/Gimmick/Cocoon.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Cocoon.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API ACocoon : public AActor
{
    GENERATED_BODY()

public:
    ACocoon();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UStaticMeshComponent* MeshComp;

    UPROPERTY()
    class ACharacter* TrappedPlayer; // 갇힌 플레이어

    FTimerHandle ExplosionTimerHandle; // 7초 타이머
    int32 HitCount = 0; // 구출 타수 카운트

    // [수정] 내용을 cpp로 옮기고 여기선 선언만 합니다.
    void Explode();

    // 플레이어 풀어주기 (공통)
    void ReleasePlayer();

    // [추가] 폭발 이펙트 (파티클)
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    class UParticleSystem* ExplosionVFX;

    // [추가] 폭발 사운드
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    class USoundBase* ExplosionSound;

public:
    // 투사체에서 이 함수를 호출해 플레이어를 가둠
    void InitializeCocoon(ACharacter* TargetPlayer);

    // 데미지 받는 함수 (아군 평타 감지)
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
};