// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/Projectile/MageIceArrow.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/BoxComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Engine/World.h"

AMageIceArrow::AMageIceArrow()
{
	ProjectileMovement->InitialSpeed = 3000.f;
}

void AMageIceArrow::Multicast_SpawnIceVFX_Implementation(FVector Location, FRotator Rotation)
{
	if (HitImpactVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			HitImpactVFX,
			Location,
			Rotation
			);
	}
	
	// [가짜 죽음 처리]
	// 부모 클래스의 MeshComp를 숨김
	if (MeshComp) MeshComp->SetVisibility(false);
	if (NiagaraComp) NiagaraComp->SetVisibility(false);
	if (ProjectileMovement) ProjectileMovement->StopMovementImmediately();
	
}


// 적을 맞췄을 때
void AMageIceArrow::OnOverlap(UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this || OtherActor == GetOwner()) return;

	if (OtherActor->ActorHasTag(TEXT("Player")))
	{
		return;
	}
	
	if (HasAuthority())
	{
		if (OtherActor->IsA(APawn::StaticClass()))
		{
			// 1. 데미지 주기
			UGameplayStatics::ApplyDamage(OtherActor, Damage, GetInstigatorController(), this, UDamageType::StaticClass());
			
			Multicast_SpawnIceVFX(OtherActor->GetActorLocation(),
				FRotator::ZeroRotator);
			
			// 2. 바닥 찾기 로직
			FVector EnemyLocation = OtherActor->GetActorLocation();
			FVector Start = EnemyLocation;
			FVector End = Start - FVector(0.0f, 0.0f, 500.0f); //아래로 5미터
			
			FHitResult GroundHit;
			FCollisionQueryParams Params;
			Params.AddIgnoredActor(OtherActor); // 적 자신은 무시 
			Params.AddIgnoredActor(this); 
			
			// 땅(WorldStatic)만 찾기
			bool bHitGround = GetWorld()->LineTraceSingleByChannel(
				GroundHit,
				Start,
				End,
				ECC_WorldStatic,
				Params
				);
			
			// 땅을 찾았으면 땅 위치, 못 찾았으면 그냥 적 위치
			FVector SpawnLoc = bHitGround ? GroundHit.ImpactPoint : EnemyLocation;
			
			// 바닥의 각도에 맞춰 회전값 계산
			// 바닥이면 Normal
			// 경사로라면 경사에 맞춰서 생성
			FRotator SpawnRot = bHitGround ? FRotationMatrix::MakeFromZ(GroundHit.ImpactNormal).Rotator() : FRotator::ZeroRotator;
			
			// 살짝 띄우기
			FVector FinalLoc = SpawnLoc + (SpawnRot.Vector() * 2.0f);
			
			// 장판 소환 (바닥에 딱 붙으면 텍스처 깨질 수 있으니 Z축 살짝 올림)
			SpawnIcePad(FinalLoc, SpawnRot);
			
			SetLifeSpan(0.1f);
			
			
		}
	}
}

// 2. 바닥/벽을 맞췄을 때
void AMageIceArrow::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	if (HasAuthority())
	{
		bool bIsAlly = Other && Other->ActorHasTag(TEXT("Player"));
		
		if (Other && (Other != this) && (Other != GetOwner()) && !bIsAlly)
		{
			Multicast_SpawnIceVFX(HitLocation, HitNormal.Rotation());
			
			FRotator WallRot = FRotationMatrix::MakeFromZ(HitNormal).Rotator();
			// 벽에서 살짝 띄워서(Normal 방향) 소환
			SpawnIcePad(HitLocation + (HitNormal * 5.0f), WallRot); 
			
			SetLifeSpan(0.1f);
		}
	}
}
// [공통 함수] 장판 소환 로직
void AMageIceArrow::SpawnIcePad(FVector SpawnLocation, FRotator SpawnRotation)
{
	if (IcePadClass)
	{
		GetWorld()->SpawnActor<AActor>(IcePadClass, SpawnLocation, SpawnRotation);
	}
}

