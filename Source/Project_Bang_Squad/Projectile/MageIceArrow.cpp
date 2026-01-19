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
		// 적이면 데미지 주기
		if (OtherActor->IsA(APawn::StaticClass()))
		{
			UGameplayStatics::ApplyDamage(OtherActor, Damage, GetInstigatorController(),
			 this, UDamageType::StaticClass());
			
			// 얼음 장판 소환
			SpawnIcePad(OtherActor->GetActorLocation());
			
			Destroy();
		}
	}
	
}

// 2. 바닥/벽을 맞췄을 때
void AMageIceArrow::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	// 부모 로직(NotifyHit) 실행 X (부모는 바로 Destroy 하니까)
    
	if (HasAuthority())
	{
		if (Other && (Other != this) && (Other != GetOwner()))
		{
			// [추가] 얼음 장판 소환! (위치는 부딪힌 지점)
			// HitLocation을 살짝 위로 띄워서 바닥에 파묻히지 않게 함
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
		FRotator SpawnRotation = FRotator::ZeroRotator; // 바닥에 평평하게
        
		// 장판 생성
		AActor* SpawnedPad = GetWorld()->SpawnActor<AActor>(IcePadClass, SpawnLocation, SpawnRotation);
		if (SpawnedPad)
		{
			// 시전자 정보 넘겨주기 (팀킬 방지 등 추후 활용 가능)
			SpawnedPad->SetInstigator(GetInstigator());
		}
	}
}
