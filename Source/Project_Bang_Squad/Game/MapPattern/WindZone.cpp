// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/Game/MapPattern/WindZone.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Components/BoxComponent.h"
#include "NiagaraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/ArrowComponent.h"
#include "Kismet/KismetSystemLibrary.h"


AWindZone::AWindZone()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	WindBox = CreateDefaultSubobject<UBoxComponent>(TEXT("WindBox"));
	RootComponent = WindBox;
	WindBox->SetBoxExtent(FVector(500.f,500.f,200.f));
	WindBox->SetCollisionProfileName(TEXT("Trigger"));

	WindVFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("WindVFX"));
	WindVFX->SetupAttachment(RootComponent);
	
	// 화살표 컴포넌트 추가
	ArrowComp = CreateDefaultSubobject<UArrowComponent>(TEXT("ArrowComp"));
	ArrowComp->SetupAttachment(RootComponent);
    
	// 화살표 크기 좀 키우기 (잘 보이게)
	ArrowComp->ArrowSize = 5.0f;
}

void AWindZone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// 1. 박스 안에 있는 모든 캐릭터 찾기
	TArray<AActor*> OverlappingActors;
	WindBox->GetOverlappingActors(OverlappingActors, ACharacter::StaticClass());
	
	// 바람 방향 벡터 (Actor의 앞방향 기준)
	FVector WindDir = GetActorForwardVector() * (bPushForward ? 1.0f : -1.0f);
	
	for (AActor* Actor : OverlappingActors)
	{
		ACharacter* TargetChar = Cast<ACharacter>(Actor);
		if (!TargetChar) continue;
		
		// 2. 차폐 판정
		// 캐릭터 위치에서 바람이 불어오는 쪽으로 레이저를 쏴봄
		FVector TraceEnd = TargetChar->GetActorLocation();
		FVector TraceStart = TraceEnd - (WindDir * 1000.0f); // 10미터 뒤에서 쏨
		
		FHitResult HitResult;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);
		
		bool bHit = GetWorld()->LineTraceSingleByChannel(
			HitResult,
			TraceStart,
			TraceEnd,
			BlockChannel, // 벽이나 방패가 걸리는 채널
			Params
			);
		
		bool bIsProtected = false;
		
		if (bHit)
		{
			AActor* HitActor = HitResult.GetActor();
			
			// 맞은게 나 자신 (캐릭터)이 아니라면 -> 뭔가에 막혔다는 뜻
			if (HitActor != TargetChar)
			{
				bIsProtected = true;
				DrawDebugLine(GetWorld(), TraceStart, HitResult.ImpactPoint, FColor::Green, false, -1.0f, 0 , 2.0f);
				
			}
		}
		
		if (!bIsProtected)
		{
			if (UCharacterMovementComponent* CMC = Cast<UCharacterMovementComponent>(TargetChar->GetCharacterMovement()))
			{
				// 기본 바람 세기
				float FinalStrength = WindStrength;

				// 땅에 있다면? -> 설정한 배율만큼 세게!
				if (CMC->IsMovingOnGround())
				{
					FinalStrength *= GroundFrictionMultiplier;
				}

				CMC->AddForce(WindDir * FinalStrength);
			}
		}
	}

}

// 1. 들어올 때 브레이크 풀기
void AWindZone::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(OtherActor))
	{
		BaseChar->SetWindResistance(true); // 브레이크 해제!
	}
}

void AWindZone::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);

	if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(OtherActor))
	{
		BaseChar->SetWindResistance(false); // 브레이크 복구!
	}
}