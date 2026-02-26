#pragma once

#include "CoreMinimal.h"
#include "SlashProjectile.h" // ✅ 1. 부모 헤더 포함
#include "LichProjectile.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;

UCLASS()
class PROJECT_BANG_SQUAD_API ALichProjectile : public ASlashProjectile // ✅ 2. 상속 변경
{
	GENERATED_BODY()

public:
	ALichProjectile();

protected:
	virtual void BeginPlay() override;

	// ✅ 3. 부모(Slash)가 데미지와 파괴를 알아서 하므로, 여기선 이펙트만 터뜨릴 함수 추가
	UFUNCTION()
	void OnLichOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

public:
	// (충돌체, 이동 컴포넌트, 데미지는 부모 것을 그대로 씁니다! 선언 안 해도 됨)

	// 리치 전용 나이아가라
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNiagaraComponent> NiagaraComp; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TObjectPtr<UNiagaraSystem> HitVFX; 
};