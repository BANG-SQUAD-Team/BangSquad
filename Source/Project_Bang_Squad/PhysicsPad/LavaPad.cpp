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
		if (TargetCharacters.Contains(Character)) 
		{
			// 꺼지기로 예약되어 있던 타이머를 취소하고 계속 불타게 만듦
			if (FTimerHandle* FoundTimer = BurnOutTimers.Find(Character))
			{
				GetWorldTimerManager().ClearTimer(*FoundTimer);
				BurnOutTimers.Remove(Character);
			}
           
			// 물리력(속도)만 다시 용암 속도로 늦춥니다.
			Character->ApplySlowDebuff(true, SpeedMultiplier);
			Character->GetCharacterMovement()->GroundFriction = 10.0f; 
			Character->GetCharacterMovement()->BrakingDecelerationWalking = 8000.0f; 
			return;
		}

		
		TargetCharacters.Add(Character);
		Character->ApplySlowDebuff(true, SpeedMultiplier);

		OriginalGroundFrictions.Add(Character, Character->GetCharacterMovement()->GroundFriction);
		OriginalBrakingDecelerations.Add(Character, Character->GetCharacterMovement()->BrakingDecelerationWalking);

		Character->GetCharacterMovement()->GroundFriction = 10.0f; 
		Character->GetCharacterMovement()->BrakingDecelerationWalking = 8000.0f; 

		if (!GetWorldTimerManager().IsTimerActive(DamageTimerHandle))
		{
			GetWorldTimerManager().SetTimer(DamageTimerHandle, this, &ALavaPad::ApplyBurnDamage, DamageInterval, true);
			ApplyBurnDamage(); // 들어오자마자 즉시 1틱 쾅!
		}
	}
}

void ALavaPad::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ABaseCharacter* Character = Cast<ABaseCharacter>(OtherActor);
	if (!Character || !TargetCharacters.Contains(Character)) return;
	
	// 1. [속도 복구] Debuff 해제 (false)
	Character->ApplySlowDebuff(false, 1.0f);
	
	
	// 2. [물리 복구] 원래 값으로 되돌림
	if (float* Friction = OriginalGroundFrictions.Find(Character))
		Character->GetCharacterMovement()->GroundFriction = *Friction;
	if (float* Braking = OriginalBrakingDecelerations.Find(Character))
		Character->GetCharacterMovement()->BrakingDecelerationWalking = *Braking;
	
	OriginalGroundFrictions.Remove(Character);
	OriginalBrakingDecelerations.Remove(Character);
	
	// [3초 도트 화상 시작]
	FTimerHandle BurnTimer;
	FTimerDelegate TimerDel;
	TimerDel.BindUFunction(this, FName("RemoveCharacterFromBurn"), Character);

	GetWorldTimerManager().SetTimer(BurnTimer,TimerDel,BurnDurationAfterExit, false);
	BurnOutTimers.Add(Character,BurnTimer);
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
    
	// 개별적으로 돌고 있던 3초 타이머들도 싹 다 취소해줍니다.
	for (auto& Pair : BurnOutTimers)
	{
		GetWorldTimerManager().ClearTimer(Pair.Value);
	}
	BurnOutTimers.Empty();
	TargetCharacters.Empty();
	OriginalGroundFrictions.Empty();
	OriginalBrakingDecelerations.Empty();
}

void ALavaPad::RemoveCharacterFromBurn(ABaseCharacter* Character)
{
	if (Character)
	{
		// 1. 화상 타겟 명단에서 제거 (이제 데미지 안 들어감)
		TargetCharacters.Remove(Character);
        
		// 2. 타이머 명단에서도 제거
		BurnOutTimers.Remove(Character);
	}

	// 만약 아무도 불타고 있는 사람이 없다면 메인 타이머(DamageTimer)를 완전히 끕니다. (최적화)
	if (TargetCharacters.IsEmpty())
	{
		GetWorldTimerManager().ClearTimer(DamageTimerHandle);
	}
}