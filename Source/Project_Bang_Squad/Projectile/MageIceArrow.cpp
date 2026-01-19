// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/Projectile/MageIceArrow.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

AMageIceArrow::AMageIceArrow()
{
	ProjectileMovement->InitialSpeed = 2500.f;
}

// 적을 맞췄을 때
void AMageIceArrow::OnOverlap(UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this || OtherActor == GetOwner()) return;

	if (HasAuthority())
	{
		if (OtherActor->IsA(APawn::StaticClass()))
		{
			UGameplayStatics::ApplyDamage(OtherActor, Damage, GetInstigatorController(), this, UDamageType::StaticClass());
			SpawnIcePad(OtherActor->GetActorLocation());
			Destroy();
		}
	}
}

// 2. 바닥/벽을 맞췄을 때
void AMageIceArrow::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	if (HasAuthority())
	{
		if (Other && (Other != this) && (Other != GetOwner()))
		{
			// 벽에서 살짝 띄워서(Normal 방향) 소환
			SpawnIcePad(HitLocation + (HitNormal * 5.0f)); 
			Destroy();
		}
	}
}
// [공통 함수] 장판 소환 로직
void AMageIceArrow::SpawnIcePad(FVector SpawnLocation)
{
	if (IcePadClass)
	{
		GetWorld()->SpawnActor<AActor>(IcePadClass, SpawnLocation, FRotator::ZeroRotator);
	}
}
