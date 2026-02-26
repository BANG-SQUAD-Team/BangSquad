#include "LichProjectile.h"
#include "Components/BoxComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
// (SlashProjectile.h는 헤더에서 이미 포함했으므로 생략)

ALichProjectile::ALichProjectile()
{
	// 부모(SlashProjectile)가 BoxComp, ProjectileMovement를 완벽하게 생성해두었습니다.
	// 여기서는 리치에 맞게 수치만 살짝 덮어씌웁니다.

	Damage = 10.0f; // 부모의 30 데미지를 리치의 10 데미지로 덮어쓰기

	if (BoxComp)
	{
		// SlashProjectile은 넓적한 모양(50,150,10)이므로, 
		// 리치의 구체 마법에 맞게 정사각형 박스로 조절해줍니다.
		BoxComp->SetBoxExtent(FVector(30.f, 30.f, 30.f)); 
	}

	// 리치 전용 나이아가라 이펙트 컴포넌트 부착
	NiagaraComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraComp"));
	NiagaraComp->SetupAttachment(RootComponent);
}

void ALichProjectile::BeginPlay()
{
	// ✅ 부모의 BeginPlay를 부르면 SlashProjectile의 완벽한 데미지 로직(OnOverlap)이 자동 바인딩됩니다.
	Super::BeginPlay(); 

	// 부모 로직과 동시에 "리치 전용 이펙트"도 터지도록 하나 더 바인딩 해줍니다.
	if (HasAuthority() && BoxComp)
	{
		BoxComp->OnComponentBeginOverlap.AddDynamic(this, &ALichProjectile::OnLichOverlap);
	}
}

void ALichProjectile::OnLichOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 데미지 계산과 삭제(Destroy)는 부모인 SlashProjectile이 100% 완벽하게 처리해 줍니다.
	// 여기서는 화면에 이펙트만 예쁘게 터뜨리면 됩니다!

	if (!OtherActor || OtherActor == GetOwner() || OtherActor == this) return;

	// 이펙트 펑!
	if (HitVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), HitVFX, GetActorLocation());
	}
}