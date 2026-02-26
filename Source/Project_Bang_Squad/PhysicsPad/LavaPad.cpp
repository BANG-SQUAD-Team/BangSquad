#include "Project_Bang_Squad/PhysicsPad/LavaPad.h"
#include "Components/StaticMeshComponent.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h" 
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Components/SkeletalMeshComponent.h"

ALavaPad::ALavaPad()
{
	PrimaryActorTick.bCanEverTick = false;

	FloorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FloorMesh"));
	RootComponent = FloorMesh;
	FloorMesh->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	FloorMesh->SetGenerateOverlapEvents(true);
}

void ALavaPad::BeginPlay()
{
	Super::BeginPlay();

	if (FloorMesh)
	{
		FloorMesh->OnComponentBeginOverlap.AddDynamic(this, &ALavaPad::OnOverlapBegin);
		FloorMesh->OnComponentEndOverlap.AddDynamic(this, &ALavaPad::OnOverlapEnd);
	}
}

void ALavaPad::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ABaseCharacter* Character = Cast<ABaseCharacter>(OtherActor);

	if (Character && Character->GetCharacterMovement())
	{
		if (TargetCharacters.Contains(Character)) return;

		TargetCharacters.Add(Character);
		
		Character->ApplySlowDebuff(true, SpeedMultiplier);

		
		
		OriginalGroundFrictions.Add(Character, Character->GetCharacterMovement()->GroundFriction);
		OriginalBrakingDecelerations.Add(Character, Character->GetCharacterMovement()->BrakingDecelerationWalking);
		//OriginalGroundFriction = Character->GetCharacterMovement()->GroundFriction;
		//OriginalBrakingDeceleration = Character->GetCharacterMovement()->BrakingDecelerationWalking;

		Character->GetCharacterMovement()->GroundFriction = 10.0f; 
		Character->GetCharacterMovement()->BrakingDecelerationWalking = 8000.0f; 

		GetWorldTimerManager().ClearTimer(ExtinguishTimerHandle);
		if (!GetWorldTimerManager().IsTimerActive(DamageTimerHandle))
		{
			GetWorldTimerManager().SetTimer(DamageTimerHandle, this, &ALavaPad::ApplyBurnDamage, DamageInterval, true);
			ApplyBurnDamage();
		}
	}
}

void ALavaPad::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ABaseCharacter* Character = Cast<ABaseCharacter>(OtherActor);
	if (!Character || !TargetCharacters.Contains(Character)) return;

	TargetCharacters.Remove(Character);
	// 1. [속도 복구] Debuff 해제 (false)
	Character->ApplySlowDebuff(false, 1.0f);
	
	
	
	// 2. [물리 복구] 원래 값으로 되돌림
	if (float* Friction = OriginalGroundFrictions.Find(Character))
		Character->GetCharacterMovement()->GroundFriction = *Friction;
	if (float* Braking = OriginalBrakingDecelerations.Find(Character))
		Character->GetCharacterMovement()->BrakingDecelerationWalking = *Braking;
	OriginalGroundFrictions.Remove(Character);
	OriginalBrakingDecelerations.Remove(Character);

	
	if (TargetCharacters.IsEmpty())
	{
		// 3. 3초 뒤 화상 종료 예약
		GetWorldTimerManager().SetTimer(ExtinguishTimerHandle, this, &ALavaPad::StopBurn, BurnDurationAfterExit, false);
	}
}

void ALavaPad::ApplyBurnDamage()
{
	if (TargetCharacters.IsEmpty())
	{
		StopBurn();
		return;
	}
    
	for (ABaseCharacter* Character : TargetCharacters)
	{
		if (IsValid(Character))
		{
			// 1. 데미지 적용
			UGameplayStatics::ApplyDamage(Character, DamageAmount, nullptr, this, UDamageType::StaticClass());    
          
			// =================================================================
			// 데미지가 들어가는 순간 몸을 빨갛게 만듦!
			// =================================================================
			if (LavaOverlayMaterial && Character->GetMesh())
			{
				// 빨간색 씌우기
				Character->GetMesh()->SetOverlayMaterial(LavaOverlayMaterial);
              
				// 0.2초 뒤에 다시 원래 색깔로 되돌리기 (깜빡!)
				FTimerHandle BlinkTimerHandle;
				Character->GetWorldTimerManager().SetTimer(
					BlinkTimerHandle, 
					FTimerDelegate::CreateWeakLambda(Character, [Character]()
					{
						if (IsValid(Character) && Character->GetMesh())
						{
							Character->GetMesh()->SetOverlayMaterial(nullptr);
						}
					}), 
					0.2f, // 0.2초 뒤에 실행 (이 숫자를 조절해서 깜빡이는 길이를 조절하세요)
					false // 반복 안 함
				);
			}
		}
	}
}

void ALavaPad::StopBurn()
{
	GetWorldTimerManager().ClearTimer(DamageTimerHandle);
	GetWorldTimerManager().ClearTimer(ExtinguishTimerHandle);
	TargetCharacters.Empty();
	OriginalGroundFrictions.Empty();
	OriginalBrakingDecelerations.Empty();
}