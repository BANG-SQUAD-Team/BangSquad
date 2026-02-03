#include "PlatformSpawner.h"

APlatformSpawner::APlatformSpawner()
{
	PrimaryActorTick.bCanEverTick = false; // 틱은 필요 없음
    bReplicates = true;
    SetReplicateMovement(true);

	// 씬 컴포넌트를 루트로 설정하여 위치 이동 가능하게 함
	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
}

void APlatformSpawner::BeginPlay()
{
	Super::BeginPlay();

    if (!HasAuthority())
    {
        return;
    }

	// 클래스가 지정되지 않았으면 실행 취소
	if (!RealPlatformClass || !TrapPlatformClass)
	{
		UE_LOG(LogTemp, Error, TEXT("Platform classes are not set in PlatformSpawner!"));
		return;
	}

	// 3줄 생성
	for (int32 i = 0; i < RowCount; i++)
	{
		SpawnPlatformRow(i);
	}
}

void APlatformSpawner::SpawnPlatformRow(int32 RowIndex)
{
    UWorld* World = GetWorld();
    if (!World) return;

    // 0 또는 1 중 랜덤하게 정답(진짜 발판) 위치 선정
    int32 SafeIndex = FMath::RandRange(0, 1);

    // 가로로 2개 배치
    for (int32 ColIndex = 0; ColIndex < 2; ColIndex++)
    {
        TSubclassOf<AActor> ClassToSpawn = (ColIndex == SafeIndex) ? RealPlatformClass : TrapPlatformClass;

        FVector SpawnLocation = GetActorLocation()
            + (GetActorForwardVector() * RowIndex * Spacing)
            + (GetActorRightVector() * ColIndex * Spacing);

        // 랜덤 회전 추가
        FRotator SpawnRotation = GetActorRotation();

        // 0도 ~ 360도 사이의 랜덤한 값을 Z축(Yaw)에 더해줍니다.
        float RandomYaw = FMath::RandRange(0.0f, 360.0f);
        SpawnRotation.Yaw += RandomYaw;

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        // 겹쳐도 무조건 생성되도록 설정 (안전장치)
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        World->SpawnActor<AActor>(ClassToSpawn, SpawnLocation, SpawnRotation, SpawnParams);
    }
}