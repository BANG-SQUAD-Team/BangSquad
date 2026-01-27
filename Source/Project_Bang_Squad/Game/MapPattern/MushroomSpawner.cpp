#include "Project_Bang_Squad/Game/MapPattern/MushroomSpawner.h"
#include "TimerManager.h"

AMushroomSpawner::AMushroomSpawner()
{
	//  감시를 이벤트로 하므로 Tick은 끕니다. (성능 최적화)
	PrimaryActorTick.bCanEverTick = false; 
	bReplicates = true;
}

void AMushroomSpawner::BeginPlay()
{
	Super::BeginPlay();
	// 서버에서만 생성
	if (HasAuthority())
	{
		SpawnMushroom();
	}
}

// Tick 함수는 이제 필요 없으므로 지워버리세요!

void AMushroomSpawner::SpawnMushroom()
{
	if (!HasAuthority()) return;
	if (!MushroomBPClass) return;
    
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    
	// 버섯 생성
	CurrentMushroom = GetWorld()->SpawnActor<AActor>(
	   MushroomBPClass,
	   GetActorLocation(),
	   GetActorRotation(),
	   SpawnParams
	   );

	//  생성된 버섯에게 "너 파괴되면 나한테 알려줘"라고 등록
	if (CurrentMushroom)
	{
		CurrentMushroom->OnDestroyed.AddDynamic(this, &AMushroomSpawner::OnMushroomDestroyed);
	}
}

// 버섯이 파괴되면 이 함수가 자동으로 실행됨
void AMushroomSpawner::OnMushroomDestroyed(AActor* DestroyedActor)
{
	// 서버에서만 로직 처리
	if (!HasAuthority()) return;

	// 내가 관리하던 그 버섯이 맞는지 확인
	if (CurrentMushroom == DestroyedActor)
	{
		CurrentMushroom = nullptr; // 포인터 초기화

		// 리필 타이머 시작 (RespawnDelay 시간 뒤에 SpawnMushroom 실행)
		// (Tick 없이도 정확한 타이밍에 실행됩니다)
		GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &AMushroomSpawner::SpawnMushroom, RespawnDelay, false);
	}
}