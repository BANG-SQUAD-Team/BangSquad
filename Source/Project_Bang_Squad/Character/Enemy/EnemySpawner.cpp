
#include "Project_Bang_Squad/Character/Enemy/EnemySpawner.h"
#include "Project_Bang_Squad/Character/Enemy/EnemyNormal.h"
#include "Components/BoxComponent.h"
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
}

void AEnemySpawner::SpawnEnemy()
{
	if (!EnemyClass) return;
	
	// 총 소환량 제한 체크 (TotalSpawnLimit가 0이면 무한)
	if (TotalSpawnLimit > 0 && TotalSpawnedCount >= TotalSpawnLimit)
	{
		// 목표 달성! 스포너 끄기
		SetSpawnerActive(false);
		return;
	}
	
	// 동시 유지 제한 체크 (너무 많이 쌓여 있으면 잠시 대기)
	if (CurrentAliveCount >= MaxConcurrentEnemyCount) return;
	
	for (int32 i = 0 ; i < SpawnCountAtOnce; i++)
	{
		// 반복문 도중에도 제한 체크
		if (TotalSpawnLimit > 0 && TotalSpawnedCount >= TotalSpawnLimit)
		{
			SetSpawnerActive(false);
			break;
		}
		if (CurrentAliveCount >= MaxConcurrentEnemyCount) break;
		
		FVector SpawnLocation = GetRandomPointInVolume();
		if (SpawnLocation.IsZero()) continue;
		
		FRotator SpawnRotation = FRotator(0.f, FMath::RandRange(0.f, 360.f), 0.f);
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		
		AEnemyNormal* NewEnemy = GetWorld()->SpawnActor<AEnemyNormal>(EnemyClass, SpawnLocation, SpawnRotation, SpawnParams);
		
		if (NewEnemy)
		{
			CurrentAliveCount++; // 현재 살아있는 수 +1
			TotalSpawnedCount++; // 누적 소환 수 +1
			
			NewEnemy->OnDestroyed.AddDynamic(this, &AEnemySpawner::OnEnemyDestroyed);
		}
	}
}

FVector AEnemySpawner::GetRandomPointInVolume()
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
			FVector FinalLocation = HitResult.ImpactPoint + FVector(0.0f, 0.0f, 90.0f);
			
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
}

void AEnemySpawner::OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) return;
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
