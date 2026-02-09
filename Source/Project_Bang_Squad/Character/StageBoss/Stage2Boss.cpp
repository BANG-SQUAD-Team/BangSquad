#include "Project_Bang_Squad/Character/StageBoss/Stage2Boss.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Project_Bang_Squad/Game/Base/BSPlayerController.h" 
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBossData.h"
#include "Project_Bang_Squad/Projectile/WebProjectile.h"

AStage2Boss::AStage2Boss()
{
	GetCapsuleComponent()->InitCapsuleSize(120.f, 120.f);

	// bIsInvincible은 부모(StageBossBase) 변수입니다.
	bIsInvincible = false;
	bShieldActive = false;
	bHasTriggeredLastStand = false;
	LivingMinionCount = 0;
}

void AStage2Boss::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// bShieldActive만 여기서 등록 (bIsInvincible은 부모에서 처리됨)
	DOREPLIFETIME(AStage2Boss, bShieldActive);
}

void AStage2Boss::BeginPlay()
{
	Super::BeginPlay();

	// BossData 변수가 이제 Stage2Boss.h에 선언되었으므로 사용 가능
	if (HasAuthority() && BossData)
	{
		
		GetCharacterMovement()->MaxWalkSpeed = BossData->WalkSpeed;
	}
}

float AStage2Boss::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority()) return 0.0f;

	// 1. 무적 상태 체크 (부모 변수 사용)
	if (bIsInvincible) return 0.0f;

	// 2. 부모 로직 태우기
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

// --- [1. 기본 공격] ---
void AStage2Boss::PerformBasicAttack(int32 AttackIndex)
{
	if (!HasAuthority()) return;

	if (BossData && BossData->AttackMontages.IsValidIndex(AttackIndex))
	{
		UAnimMontage* Montage = BossData->AttackMontages[AttackIndex];
		PlayAnimMontage(Montage);
		Multicast_PlayMontage(Montage);
	}
}

void AStage2Boss::ExecuteAttackTrace(FName SocketName, float AttackRadius)
{
	if (!HasAuthority()) return;

	FVector TraceStart = GetMesh()->GetSocketLocation(SocketName);
	FVector TraceEnd = TraceStart + (GetActorForwardVector() * 150.0f) + (FVector::DownVector * 50.0f);

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(this);

	TArray<FHitResult> OutHits;

	bool bHit = UKismetSystemLibrary::SphereTraceMultiForObjects(
		GetWorld(),
		TraceStart,
		TraceEnd,
		AttackRadius,
		{ UEngineTypes::ConvertToObjectType(ECC_Pawn) },
		false,
		ActorsToIgnore,
		EDrawDebugTrace::ForDuration,
		OutHits,
		true
	);

	if (bHit)
	{
		for (const FHitResult& Hit : OutHits)
		{
			if (AActor* HitActor = Hit.GetActor())
			{
				// AttackDamage는 부모(EnemyBaseData)에 있다고 가정
				float DamageVal = (BossData) ? BossData->AttackDamage : 10.0f;
				UGameplayStatics::ApplyDamage(HitActor, DamageVal, GetController(), this, UDamageType::StaticClass());
			}
		}
	}
}

// --- [2. 백스텝] ---
void AStage2Boss::PerformBackstep()
{
	if (!HasAuthority()) return;
	FVector BackVelocity = -GetActorForwardVector() * 1500.0f + FVector::UpVector * 300.0f;
	LaunchCharacter(BackVelocity, true, true);
}

// --- [3. 거미줄 발사] ---
void AStage2Boss::PerformWebShot(AActor* TargetActor)
{
	// [수정] WebProjectileClass 대신 BossData 체크
	if (!HasAuthority() || !TargetActor || !BossData || !BossData->WebProjectileClass) return;

	// 1. 발사 몽타주 재생
	if (BossData->WebShotMontage)
	{
		PlayAnimMontage(BossData->WebShotMontage);
		Multicast_PlayMontage(BossData->WebShotMontage);
	}

	// 2. 투사체 생성
	FVector SpawnLoc = GetMesh()->GetSocketLocation(TEXT("MouthSocket"));
	if (SpawnLoc.IsZero()) SpawnLoc = GetActorLocation() + GetActorForwardVector() * 100.0f + FVector(0, 0, 50);

	FRotator SpawnRot = (TargetActor->GetActorLocation() - SpawnLoc).Rotation();

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = GetInstigator();

	// [핵심 변경] BossData에서 클래스를 가져와 스폰
	// BossData->WebProjectileClass는 TSubclassOf<AActor>이므로 SpawnActor<AActor> 사용
	GetWorld()->SpawnActor<AActor>(BossData->WebProjectileClass, SpawnLoc, SpawnRot, Params);
}

// --- [4. QTE 패턴] ---
void AStage2Boss::StartQTEPattern(AActor* TargetPlayer)
{
	if (!HasAuthority()) return;

	ABSPlayerController* TargetPC = Cast<ABSPlayerController>(Cast<APawn>(TargetPlayer)->GetController());
	if (TargetPC)
	{
		FTimerDelegate TimerDel;
		TimerDel.BindLambda([this, TargetPC]()
			{
				UE_LOG(LogTemp, Warning, TEXT("QTE Failed!"));
			});

		GetWorld()->GetTimerManager().SetTimer(TimerHandle_QTEFail, TimerDel, 5.2f, false);
	}
}

void AStage2Boss::ServerRPC_QTEResult_Implementation(APlayerController* Player, bool bSuccess)
{
	if (GetWorld()->GetTimerManager().IsTimerActive(TimerHandle_QTEFail))
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_QTEFail);
		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("QTE Success!"));
		}
	}
}

// --- [5. 전멸기] ---
void AStage2Boss::StartSplitPattern()
{
	if (!HasAuthority()) return;

	bIsInvincible = true;
	bShieldActive = true;

	SetActorLocation(FVector(0, 0, 1000));

	FTimerHandle WipeTimer;
	GetWorld()->GetTimerManager().SetTimer(WipeTimer, this, &AStage2Boss::CheckSplitResult, 5.0f, false);
}

void AStage2Boss::CheckSplitResult()
{
	if (!HasAuthority()) return;

	bool bSuccess = false; // 로직 구현 필요

	if (bSuccess)
	{
		bIsInvincible = false;
		bShieldActive = false;
		Multicast_ShowWipeEffect(true);
	}
	else
	{
		Multicast_ShowWipeEffect(false);
	}
}

// --- [6. 소환] ---
void AStage2Boss::CheckSpawnPhase(float NewHP) {}

void AStage2Boss::SpawnMinions()
{
	bIsInvincible = true;
	bShieldActive = true;
}

void AStage2Boss::OnMinionDestroyed(AActor* DestroyedActor)
{
	LivingMinionCount--;
	if (LivingMinionCount <= 0)
	{
		bIsInvincible = false;
		bShieldActive = false;
	}
}

void AStage2Boss::OnRep_ShieldActive()
{
	if (bShieldActive)
	{
		UE_LOG(LogTemp, Log, TEXT("Boss Shield ON"));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Boss Shield OFF"));
	}
}

void AStage2Boss::Multicast_PlayMontage_Implementation(UAnimMontage* MontageToPlay)
{
	if (HasAuthority() && !IsLocallyControlled()) return;
	if (MontageToPlay) PlayAnimMontage(MontageToPlay);
}

void AStage2Boss::Multicast_ShowWipeEffect_Implementation(bool bSuccess)
{
	// 전멸기 이펙트
}