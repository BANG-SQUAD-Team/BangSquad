#include "PillarRotate.h"
#include "Components/SphereComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h" 
#include "Curves/CurveFloat.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Character.h" 
#include "Kismet/GameplayStatics.h" 

APillarRotate::APillarRotate()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	HingeHelper = CreateDefaultSubobject<USphereComponent>(TEXT("HingeHelper"));
	HingeHelper->SetupAttachment(SceneRoot);
	HingeHelper->SetSphereRadius(20.0f);
	HingeHelper->bIsEditorOnly = true;

	DirectionHelper = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionHelper"));
	DirectionHelper->SetupAttachment(SceneRoot);
	DirectionHelper->bIsEditorOnly = true;

	PillarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PillarMesh"));
	PillarMesh->SetupAttachment(SceneRoot);

	DamageTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("DamageTrigger"));
	DamageTrigger->SetupAttachment(PillarMesh); // 메시에 붙어야 같이 회전함
	DamageTrigger->SetBoxExtent(FVector(50.f, 50.f, 200.f)); // 기둥 크기에 맞춰 조절 필요
	DamageTrigger->SetCollisionProfileName(TEXT("OverlapAllDynamic")); // 겹침 감지용
	DamageTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void APillarRotate::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && DamageTrigger)
	{
		DamageTrigger->OnComponentBeginOverlap.AddDynamic(this, &APillarRotate::OnDamageTriggerOverlap);
	}
}

void APillarRotate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// 모든 클라이언트가 동일한 애니메이션 상태를 갖도록 복제 등록
	DOREPLIFETIME(APillarRotate, bIsFalling);
	DOREPLIFETIME(APillarRotate, StartTime);
	DOREPLIFETIME(APillarRotate, ReplicatedRotationAxis);
	DOREPLIFETIME(APillarRotate, ReplicatedInitialRotation);
	DOREPLIFETIME(APillarRotate, ReplicatedInitialLocation);
}

void APillarRotate::ProcessMageInput(FVector Direction)
{
	if (!HasAuthority()) return; // 서버에서만 판정
	if (bIsFalling || StartTime > 0.0f) return;

	if (Direction.Y > 0.4f)
	{
		// [핵심] 상호작용 시점의 월드 상태를 캡처하여 고정합니다.
		if (AGameStateBase* GS = GetWorld()->GetGameState())
		{
			StartTime = GS->GetServerWorldTimeSeconds();
		}

		ReplicatedInitialRotation = GetActorRotation();
		ReplicatedInitialLocation = GetActorLocation();

		if (DirectionHelper)
		{
			// DirectionHelper의 방향을 기준으로 회전축을 서버에서 확정합니다.
			ReplicatedRotationAxis = DirectionHelper->GetRightVector();
		}
		else
		{
			ReplicatedRotationAxis = GetActorRightVector();
		}

		ReplicatedRotationAxis = ReplicatedRotationAxis.GetSafeNormal();

		bIsFalling = true;
		SetActorTickEnabled(true);

		if (DamageTrigger)
		{
			DamageTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			HitActors.Empty();
		}

		SetMageHighlight(false);
		if (PillarMesh)
		{
			PillarMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		}
	}
}

void APillarRotate::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 서버 시간이 복제되었다면 애니메이션 계산 시작
	if (StartTime > 0.f)
	{
		if (AGameStateBase* GS = GetWorld()->GetGameState())
		{
			float ServerTime = GS->GetServerWorldTimeSeconds();
			float ElapsedTime = ServerTime - StartTime;

			float Alpha = FMath::Clamp(ElapsedTime / FallDuration, 0.0f, 1.0f);

			float TargetAlpha = Alpha;
			if (FallCurve)
			{
				// 커브도 현재 경과 시간을 기준으로 샘플링합니다.
				TargetAlpha = FallCurve->GetFloatValue(ElapsedTime);
			}

			UpdateFallProgress(TargetAlpha);

			// 애니메이션이 완료되면 틱을 종료하여 성능 최적화
			if (ElapsedTime >= FallDuration)
			{
				if (DamageTrigger)
				{
					DamageTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				}
				SetActorTickEnabled(false);
			}
		}
	}
}

void APillarRotate::UpdateFallProgress(float Alpha)
{
	// 서버에서 복제된 축과 초기값을 사용하여 쿼터니언 회전 적용
	FQuat DeltaRotation = FQuat(ReplicatedRotationAxis, FMath::DegreesToRadians(MaxFallAngle * Alpha));
	SetActorRotation(DeltaRotation * ReplicatedInitialRotation.Quaternion());

	// 위치 보간
	FVector TargetLoc = ReplicatedInitialLocation + ReplicatedInitialRotation.RotateVector(FallLocationOffset);
	SetActorLocation(FMath::Lerp(ReplicatedInitialLocation, TargetLoc, Alpha));
}

void APillarRotate::OnRep_bIsFalling()
{
	if (bIsFalling)
	{
		SetActorTickEnabled(true);
		SetMageHighlight(false);
	}
}

void APillarRotate::SetMageHighlight(bool bActive)
{
	if (PillarMesh)
	{
		if (bIsFalling || StartTime > 0.0f)
		{
			PillarMesh->SetRenderCustomDepth(false);
			return;
		}
		PillarMesh->SetRenderCustomDepth(bActive);
		PillarMesh->SetCustomDepthStencilValue(bActive ? 250 : 0);
	}
}

void APillarRotate::OnDamageTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 1. 유효성 체크
	if (!OtherActor || OtherActor == this || HitActors.Contains(OtherActor)) return;

	ACharacter* HitCharacter = Cast<ACharacter>(OtherActor);
	if (HitCharacter)
	{
		HitActors.Add(OtherActor); // 중복 피격 방지

		// [디버그 로그] 화면에 빨간 글씨로 띄워서 맞았는지 확인
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, FString::Printf(TEXT("HIT: %s"), *HitCharacter->GetName()));

		// 2. 데미지 적용
		UGameplayStatics::ApplyDamage(HitCharacter, DamageAmount, GetInstigatorController(), this, UDamageType::StaticClass());

		// 3. 넉백 방향 계산 (더 단순하고 강력하게)
		FVector LaunchDir;

		// 기둥이 넘어지는 방향(DirectionHelper)이 있으면 그 방향으로, 없으면 플레이어 뒤로
		if (DirectionHelper)
		{
			LaunchDir = DirectionHelper->GetForwardVector();
		}
		else
		{
			// 기둥 중심 -> 플레이어 방향
			LaunchDir = (HitCharacter->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		}

		// [핵심 수정] 위쪽(Z) 힘을 강제로 섞습니다. (45도 각도)
		// 땅에 끌리지 않게 하려면 Z값이 0.5 이상은 되어야 안전합니다.
		LaunchDir.Z = 0.2f;
		LaunchDir = LaunchDir.GetSafeNormal();

		float FinalKnockbackForce = 1500.0f;

		// 4. 날리기 (LaunchCharacter)
		// XY, Z 모두 override(true, true)해서 강제로 궤적을 수정합니다.
		HitCharacter->LaunchCharacter(LaunchDir * FinalKnockbackForce, true, true);
	}
}