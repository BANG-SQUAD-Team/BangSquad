
#include "Project_Bang_Squad/Character/Enemy/EnemySpawner.h"
#include "Project_Bang_Squad/Character/Enemy/EnemyNormal.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"
#include "TimerManager.h"


AEnemySpawner::AEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	
	// 1. 스폰 구역
	SpawnVolume = CreateDefaultSubobject<UBoxComponent>(FName("SpawnVolume"));
	RootComponent = SpawnVolume;
	SpawnVolume->SetBoxExtent(FVector(1000.f, 1000.f, 100.f));
	SpawnVolume->SetLineThickness(5.0f);
	SpawnVolume->ShapeColor = FColor::Green;
	SpawnVolume->SetCollisionProfileName(TEXT("NoCollision"));
	
	// 2. 트리거 구역 (노란색)
	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(FName("TriggerVolume"));
	TriggerVolume->SetupAttachment(RootComponent);
	TriggerVolume->SetBoxExtent(FVector(1200.f, 1200.f, 500.f));
	TriggerVolume->SetLineThickness(3.0f);
	TriggerVolume->ShapeColor = FColor::Yellow;
	
	TriggerVolume->SetCollisionProfileName(TEXT("Trigger"));
	TriggerVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

}


void AEnemySpawner::BeginPlay()
{
	Super::BeginPlay();
	
	if (HasAuthority())
	{
		TriggerVolume->OnComponentBeginOverlap.AddDynamic(this, &AEnemySpawner::OnTriggerOverlap);
		TriggerVolume->OnComponentEndOverlap.AddDynamic(this, &AEnemySpawner::OnTriggerEndOverlap);
		
		if (bStartAutomatically)
		{
			SetSpawnerActive(true);
		}
	}
	
}

void AEnemySpawner::SetSpawnerActive(bool bActive)
{
	if (!HasAuthority()) return;
	
	// 이미 총량 제한을 다 채웠으면 다시 켜지 않음
	if (bActive && TotalSpawnLimit > 0 && TotalSpawnedCount >= TotalSpawnLimit)
	{
		return;
	}
	
	if (bIsActive == bActive) return;
	
	bIsActive = bActive;
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	
	if (bIsActive)
	{
		SpawnEnemy();
		GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &AEnemySpawner::SpawnEnemy,
			SpawnInterval, true);
	}
	else
	{
		bHasSpawnedMidBoss = false;
	}
}

void AEnemySpawner::SpawnEnemy()
{
    // [수정] 둘 다 없으면 리턴 (보스만 있고 일반몹 없는 경우도 고려)
    if (!EnemyClass && !MidBossClass) return;
    
    // 총 소환량 제한 체크
    if (TotalSpawnLimit > 0 && TotalSpawnedCount >= TotalSpawnLimit)
    {
       SetSpawnerActive(false);
       return;
    }
    
    // =========================================================
    // 1. 중간 보스 소환 로직 (1마리만)
    // =========================================================
    if (MidBossClass && !bHasSpawnedMidBoss)
    {
       // 보스의 키(CapsuleHalfHeight)를 미리 알아냄
       float BossHeight = 90.0f; // 기본값
       
       // CDO(Class Default Object)를 통해 블루프린트 설정을 미리 읽어옴
       ACharacter* BossCDO = Cast<ACharacter>(MidBossClass->GetDefaultObject());
       if (BossCDO && BossCDO->GetCapsuleComponent())
       {
           BossHeight = BossCDO->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
       }

       // 보스 키를 인자로 넘겨줌
       FVector BossSpawnLoc = GetRandomPointInVolume(BossHeight);
       
       if (!BossSpawnLoc.IsZero())
       {
          FRotator SpawnRot = FRotator(0.f, FMath::RandRange(0.f, 360.f), 0.f);
          FActorSpawnParameters SpawnParams;
          SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
       
          // 보스 소환 (MidBossClass는 이제 AEnemyCharacterBase를 받으므로 타입 맞춰줌)
          AEnemyCharacterBase* NewBoss = GetWorld()->SpawnActor<AEnemyCharacterBase>(MidBossClass,
             BossSpawnLoc, SpawnRot, SpawnParams);
       
          if (NewBoss)
          {
             bHasSpawnedMidBoss = true; // 소환 완료 체크!
                
             CurrentAliveCount++;
             TotalSpawnedCount++; // [추가] 보스도 총 소환수에 포함
                
             NewBoss->OnDestroyed.AddDynamic(this, &AEnemySpawner::OnEnemyDestroyed);
                
             // 보스를 뽑았으니 이번 턴은 종료
             return; 
          }
       }
    }
    
    // =========================================================
    // 2. 일반 몬스터 소환 로직
    // =========================================================
    
    // 일반 몹 클래스가 없으면 여기서 중단
    if (!EnemyClass) return;

    // 동시 유지 제한 체크
    if (CurrentAliveCount >= MaxConcurrentEnemyCount) return;

    //  일반 몬스터 키 미리 알아내기 (반복문 밖에서 한 번만 계산)
    float NormalHeight = 90.0f;
    ACharacter* NormalCDO = Cast<ACharacter>(EnemyClass->GetDefaultObject());
    if (NormalCDO && NormalCDO->GetCapsuleComponent())
    {
        NormalHeight = NormalCDO->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    }
    
    for (int32 i = 0 ; i < SpawnCountAtOnce; i++)
    {
       // 반복문 도중에도 제한 체크
       if (TotalSpawnLimit > 0 && TotalSpawnedCount >= TotalSpawnLimit)
       {
          SetSpawnerActive(false);
          break;
       }
       if (CurrentAliveCount >= MaxConcurrentEnemyCount) break;
       
       // 일반 몬스터 키를 인자로 넘겨줌
       FVector SpawnLocation = GetRandomPointInVolume(NormalHeight);
       
       if (SpawnLocation.IsZero()) continue;
       
       FRotator SpawnRotation = FRotator(0.f, FMath::RandRange(0.f, 360.f), 0.f);
       FActorSpawnParameters SpawnParams;
       SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
       
       AEnemyNormal* NewEnemy = GetWorld()->SpawnActor<AEnemyNormal>(EnemyClass, SpawnLocation, SpawnRotation, SpawnParams);
       
       if (NewEnemy)
       {
          CurrentAliveCount++; 
          TotalSpawnedCount++; 
          
          NewEnemy->OnDestroyed.AddDynamic(this, &AEnemySpawner::OnEnemyDestroyed);
       }
    }
}

FVector AEnemySpawner::GetRandomPointInVolume(float HalfHeight)
{
	FVector BoxOrigin = SpawnVolume->GetComponentLocation();
	FVector BoxExtent = SpawnVolume->GetScaledBoxExtent();
	
	for (int32 Attempt = 0; Attempt < 10; Attempt++)
	{
		FVector RandomPoint = UKismetMathLibrary::RandomPointInBoundingBox(BoxOrigin, BoxExtent);
		
		FHitResult HitResult;
		FVector TraceStart = RandomPoint;
		TraceStart.Z = BoxOrigin.Z + BoxExtent.Z;
		FVector TraceEnd = RandomPoint;
		TraceEnd.Z = BoxOrigin.Z - BoxExtent.Z - 1000.0f;
		
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);
		
		bool bHitGround = GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, Params);
		
		if (bHitGround)
		{
			FVector FinalLocation = HitResult.ImpactPoint + FVector(0.0f, 0.0f, HalfHeight + 5.0f);
			
			TArray<AActor*> OverlappedActors;
			TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
			ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
			
			bool bIsCrowded = UKismetSystemLibrary::SphereOverlapActors(
				GetWorld(),
				FinalLocation,
				MinSpawnDistance,
				ObjectTypes,
				ACharacter::StaticClass(),
				{ this },
				OverlappedActors
				);
			
			if (!bIsCrowded) return FinalLocation;
		}
	}
	return FVector::ZeroVector;
}

void AEnemySpawner::OnEnemyDestroyed(AActor* DestroyedActor)
{
	if (CurrentAliveCount > 0)
	{
		CurrentAliveCount--;
	}

	if (TotalSpawnLimit > 0 && TotalSpawnedCount >= TotalSpawnLimit && CurrentAliveCount <= 0)
	{
		SetSpawnerActive(false);

		if (OnSpawnerCleared.IsBound())
		{
			OnSpawnerCleared.Broadcast();
		}
	}
}

void AEnemySpawner::OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) return;
	
	if (bIsLockedByPuzzle) return; 
	
	if (OtherActor && OtherActor->ActorHasTag("Player"))
	{
		SetSpawnerActive(true);
	}
}

void AEnemySpawner::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority()) return;
	if (bKeepSpawningAfterLeave) return;
	
	if (OtherActor && OtherActor->ActorHasTag("Player"))
	{
		TArray<AActor*> OverlappingActors;
		TriggerVolume->GetOverlappingActors(OverlappingActors);
		
		bool bAnyPlayerLeft = false;
		for (AActor* Actor : OverlappingActors)
		{
			if (Actor && Actor->ActorHasTag("Player"))
			{
				bAnyPlayerLeft = true;
				break;
			}
		}
		
		if (!bAnyPlayerLeft)
		{
			SetSpawnerActive(false);
		}
	}
}

void AEnemySpawner::UnlockSpawner()
{
	if (!HasAuthority()) return;
	
	// 1. 잠금 홰제
	bIsLockedByPuzzle = false;
	
	// 2. 잠금이 풀렸을 때, 플레이어가 이미 트리거 안에 서 있는지 검사
	if (TriggerVolume)
	{
		TArray<AActor*> OverlappingActors;
		TriggerVolume->GetOverlappingActors(OverlappingActors);
		
		for (AActor* Actor : OverlappingActors)
		{
			if (Actor && Actor->ActorHasTag("Player"))
			{
				// 플레이어가 안에 있다면 즉시 스폰
				SetSpawnerActive(true);
				break;
			}
		}
	}
}