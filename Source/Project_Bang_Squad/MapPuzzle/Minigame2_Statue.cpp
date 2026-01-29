#include "Minigame2_Statue.h"
#include "Components/BoxComponent.h"
#include "Curves/CurveFloat.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"

AMinigame2_Statue::AMinigame2_Statue()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false); // 직접 계산하므로 끔

	StatueMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatueMesh"));
	RootComponent = StatueMesh;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(RootComponent);
}

void AMinigame2_Statue::BeginPlay()
{
	Super::BeginPlay();
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AMinigame2_Statue::OnOverlapBegin);
}

void AMinigame2_Statue::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMinigame2_Statue, bIsFalling);
	DOREPLIFETIME(AMinigame2_Statue, StartTime);
	DOREPLIFETIME(AMinigame2_Statue, ReplicatedInitialRotation);
	DOREPLIFETIME(AMinigame2_Statue, ReplicatedInitialLocation);
}

void AMinigame2_Statue::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || bIsFalling || StartTime > 0.f) return;

	if (OtherActor->ActorHasTag(TEXT("Player")))
	{
		if (AGameStateBase* GS = GetWorld()->GetGameState())
		{
			StartTime = GS->GetServerWorldTimeSeconds(); // 서버 시간 캡처
		}
		ReplicatedInitialRotation = GetActorRotation();
		ReplicatedInitialLocation = GetActorLocation();
		bIsFalling = true;
	}
}

void AMinigame2_Statue::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (StartTime > 0.f)
	{
		if (AGameStateBase* GS = GetWorld()->GetGameState())
		{
			float ElapsedTime = GS->GetServerWorldTimeSeconds() - StartTime;
			float TargetAlpha = FallCurve ? FallCurve->GetFloatValue(ElapsedTime) : (ElapsedTime / FallDuration);
			float Alpha = FMath::Clamp(ElapsedTime / FallDuration, 0.0f, 1.0f);

			UpdateFallProgress(TargetAlpha);

			if (HasAuthority() && Alpha >= 0.9f && !bFloorCrushed)
			{
				bFloorCrushed = true;
				TArray<AActor*> Ignore;
				Ignore.Add(this);
				UGameplayStatics::ApplyRadialDamage(GetWorld(), 5000.f, GetActorLocation(), 600.f, nullptr, Ignore, this, nullptr, false);
			}

			// [수정] 애니메이션 종료 시 물리 엔진 활성화
			if (ElapsedTime >= FallDuration)
			{
				if (HasAuthority() && StatueMesh)
				{
					// 1. 물리 시뮬레이션 및 중력 활성화
					StatueMesh->SetSimulatePhysics(true);
					StatueMesh->SetEnableGravity(true);

					// 2. 물리 동기화를 위해 다시 엔진 기본 복제 활성화 (선택 사항)
					SetReplicateMovement(true);
				}
				SetActorTickEnabled(false); // 더 이상 Tick 연산 안 함
			}
		}
	}
}

void AMinigame2_Statue::UpdateFallProgress(float Alpha)
{
	FQuat StartQuat = ReplicatedInitialRotation.Quaternion();

	FQuat DeltaQuat = FQuat(FallRotationAmount);
	FQuat TargetQuat = DeltaQuat * StartQuat;

	SetActorRotation(FQuat::Slerp(StartQuat, TargetQuat, Alpha));

	FVector TargetLoc = ReplicatedInitialLocation + ReplicatedInitialRotation.RotateVector(FallLocationOffset);
	SetActorLocation(FMath::Lerp(ReplicatedInitialLocation, TargetLoc, Alpha));
}

void AMinigame2_Statue::OnRep_bIsFalling()
{
	if (bIsFalling) SetActorTickEnabled(true);
}