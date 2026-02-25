// Source/Project_Bang_Squad/Character/StageBoss/Stage3Boss.cpp

#include "Project_Bang_Squad/Character/StageBoss/Stage3Boss/Stage3Boss.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBossData.h"
#include "Project_Bang_Squad/Character/StageBoss/Stage3Boss/PlatformManager.h"
#include "Project_Bang_Squad/Character/StageBoss/Stage3Boss/BossPlatform.h"
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/DamageEvents.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "Blueprint/UserWidget.h"
#include "Project_Bang_Squad/Core/TrueDamageType.h"

AStage3Boss::AStage3Boss()
{
	// 보스 덩치 설정
	GetMesh()->SetRelativeScale3D(FVector(2.0f));
	PrimaryActorTick.bCanEverTick = true;

	// HealthComponent가 없다면 생성 (보통 Base에 있으나 안전책)
	if (!FindComponentByClass<UHealthComponent>())
	{
		// 부모 생성자에서 만들지 않았다면 여기서 생성 (하지만 보통 Base에 있음)
	}
}

void AStage3Boss::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		// 1. BossData 적용 (체력 등)
		if (BossData)
		{
			if (UHealthComponent* HC = FindComponentByClass<UHealthComponent>())
			{
				HC->SetMaxHealth(BossData->MaxHealth);
				// 체력 변경 델리게이트 연결
				HC->OnHealthChanged.AddDynamic(this, &AStage3Boss::OnHealthChanged);
			}
		}

		// 2. 발판 매니저 찾기
		if (!PlatformManager)
		{
			PlatformManager = Cast<APlatformManager>(UGameplayStatics::GetActorOfClass(GetWorld(), APlatformManager::StaticClass()));
		}

		if (PlatformManager)
		{
			PlatformManager->StartMechanics();
		}
	}

	// 3. UI 초기화 (모든 클라이언트)
	if (UHealthComponent* HC = FindComponentByClass<UHealthComponent>())
	{
		Multicast_ShowBossHP_Implementation(HC->MaxHealth);
	}
}

void AStage3Boss::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// [레이저 회전 로직]
	if (bIsFiringLaser)
	{
		// 타겟이 없거나 죽었으면 리타겟팅
		if (!LaserTargetActor || (LaserTargetActor && LaserTargetActor->IsPendingKillPending()))
		{
			FindNearestPlayer();
		}

		if (LaserTargetActor)
		{
			FVector Dir = LaserTargetActor->GetActorLocation() - GetActorLocation();
			Dir.Z = 0; // 높이는 무시하고 회전
			FRotator TargetRot = Dir.Rotation();

			// 부드럽게 회전 (InterpSpeed 3.0)
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, 3.0f));
		}
	}
}

float AStage3Boss::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// [트루 데미지 체크] 무적 무시
	bool bIsTrueDamage = (DamageEvent.DamageTypeClass == UTrueDamageType::StaticClass());

	if (!bIsTrueDamage)
	{
		// 권한 없거나 무적 상태면 데미지 0
		if (!HasAuthority() || bIsInvincible) return 0.0f;
	}

	// 부모 처리 (HealthComponent 차감 등)
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void AStage3Boss::OnHealthChanged(float CurrentHealth, float MaxHealth)
{
	if (HasAuthority())
	{
		// UI 갱신 방송
		Multicast_UpdateBossHP(CurrentHealth, MaxHealth);

		// 사망 처리
		if (CurrentHealth <= 0.0f)
		{
			Multicast_HideBossHP();
			// TODO: 사망 몽타주, 레벨 클리어 로직 등 추가
		}
	}
}

// ==============================================================================
// [Skills]
// ==============================================================================

float AStage3Boss::Execute_BasicAttack()
{
	if (!BossData) return 1.0f;

	// 30% 확률로 특수 패턴(날아가기) 등 추가 가능

	// 랜덤 평타 몽타주 재생
	UAnimMontage* AttackMontage = nullptr;
	if (BossData->WingAttackL_Montage && BossData->WingAttackR_Montage)
	{
		AttackMontage = FMath::RandBool() ? BossData->WingAttackL_Montage : BossData->WingAttackR_Montage;
	}
	else if (BossData->WingAttackL_Montage)
	{
		AttackMontage = BossData->WingAttackL_Montage;
	}

	if (AttackMontage)
	{
		Multicast_PlayBossMontage(AttackMontage);
		return AttackMontage->GetPlayLength();
	}
	return 1.0f;
}

float AStage3Boss::Execute_Laser()
{
	// 1. 타겟 선정
	FindNearestPlayer();
	bIsFiringLaser = true;

	// 2. 몽타주 재생
	if (BossData && BossData->LaserMontage)
	{
		Multicast_PlayBossMontage(BossData->LaserMontage);
	}

	// 3. 데미지 타이머 시작 (0.25초 간격)
	GetWorldTimerManager().SetTimer(LaserDamageTimer, this, &AStage3Boss::ApplyLaserDamage, 0.25f, true);

	// 4. 3초 후 종료 처리
	FTimerHandle EndHandle;
	GetWorldTimerManager().SetTimer(EndHandle, [this]()
		{
			bIsFiringLaser = false;
			GetWorldTimerManager().ClearTimer(LaserDamageTimer);
			if (GetMesh()->GetAnimInstance())
			{
				GetMesh()->GetAnimInstance()->Montage_JumpToSection(FName("End"), BossData->LaserMontage);
			}
		}, 3.0f, false);

	return 4.0f;
}

void AStage3Boss::ApplyLaserDamage()
{
	// 레이저 판정 (Box Trace)
	FVector Start = GetActorLocation();
	FVector End = Start + (GetActorForwardVector() * 2000.0f); // 사거리 2000

	TArray<FHitResult> Hits;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	bool bHit = GetWorld()->SweepMultiByChannel(
		Hits,
		Start,
		End,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeBox(FVector(50, 50, 50)), // 두께
		Params
	);

	if (bHit)
	{
		for (auto& Hit : Hits)
		{
			if (Hit.GetActor() && Hit.GetActor()->IsA(ACharacter::StaticClass()))
			{
				// 데미지 25 적용
				UGameplayStatics::ApplyDamage(Hit.GetActor(), 25.0f, GetController(), this, UDamageType::StaticClass());
			}
		}
	}
}

float AStage3Boss::Execute_Meteor()
{
	if (!PlatformManager || !BossData) return 2.0f;

	// 1. 몽타주 재생 (캐스팅 동작) - Multicast 적용
	float Duration = 0.0f;
	if (BossData->MeteorMontage)
	{
		Multicast_PlayBossMontage(BossData->MeteorMontage, FName("Cast"));
		Duration = BossData->MeteorMontage->GetPlayLength();
	}

	// 2. 타겟 발판 선정
	TArray<ABossPlatform*> Targets = PlatformManager->GetPlatformsForMeteor();

	// 3. 0.5초 뒤 경고 표시 (붉은색 장판 켜기)
	FTimerHandle WarningTimer;
	GetWorldTimerManager().SetTimer(WarningTimer, [Targets]()
		{
			for (ABossPlatform* P : Targets)
			{
				if (P) P->SetWarning(true);
			}
		}, 0.5f, false);

	// 4. 시전 3초 후: 폭발 이펙트 + 광역 데미지 + 넉백 + 즉시 하강
	FTimerHandle ImpactTimer;
	GetWorldTimerManager().SetTimer(ImpactTimer, [this, Targets]()
		{
			for (ABossPlatform* P : Targets)
			{
				if (P)
				{
					// 폭발 중심점 (발판 살짝 위)
					FVector ExplosionCenter = P->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);

					// 비주얼 동기화
					Multicast_SpawnMeteorFX(ExplosionCenter);

					// 데미지 및 넉백 판정
					float DamageRadius = 450.0f;
					float MeteorDamage = 100.0f;
					float KnockbackForce = 1500.0f;

					TArray<FOverlapResult> OverlapResults;
					FCollisionQueryParams QueryParams;
					QueryParams.AddIgnoredActor(this);

					bool bHit = GetWorld()->OverlapMultiByChannel(
						OverlapResults, ExplosionCenter, FQuat::Identity, ECC_Pawn,
						FCollisionShape::MakeSphere(DamageRadius), QueryParams
					);

					if (bHit)
					{
						for (const FOverlapResult& Overlap : OverlapResults)
						{
							ACharacter* HitPlayer = Cast<ACharacter>(Overlap.GetActor());
							if (IsValid(HitPlayer))
							{
								// 데미지 적용
								UGameplayStatics::ApplyDamage(HitPlayer, MeteorDamage, GetController(), this, UDamageType::StaticClass());

								// 넉백 적용
								FVector KnockbackDir = (HitPlayer->GetActorLocation() - ExplosionCenter).GetSafeNormal();
								KnockbackDir.Z = 0.8f;
								KnockbackDir.Normalize();

								HitPlayer->LaunchCharacter(KnockbackDir * KnockbackForce, true, true);
							}
						}
					}

					// 발판 즉시 하강
					P->MoveDown(false);
				}
			}
		}, 3.0f, false);

	// 자막 출력
	Multicast_ShowBossSubtitle(FText::FromString(TEXT("메테오가 떨어집니다!")), 3.0f);

	// 5. (복구된 로직) 시전 3초 후 먼 적에게 다이브(Dive) 이동
	FTimerHandle DiveTimer;
	GetWorldTimerManager().SetTimer(DiveTimer, [this]()
		{
			AActor* FurthestTarget = nullptr;
			float MaxDist = -1.0f;
			TArray<AActor*> Players;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), Players);

			for (AActor* P : Players)
			{
				if (P != this)
				{
					float D = GetDistanceTo(P);
					if (D > MaxDist) { MaxDist = D; FurthestTarget = P; }
				}
			}

			if (FurthestTarget)
			{
				// 회전
				FVector Direction = (FurthestTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal();
				Direction.Z = 0.0f;
				SetActorRotation(Direction.Rotation());

				// 다이브 도약 (서버에서 LaunchCharacter 실행 시 클라이언트도 자동 동기화됨)
				FVector LaunchVel = Direction * (MaxDist * 1.0f) + FVector(0, 0, 500.0f);
				LaunchCharacter(LaunchVel, true, true);
			}

			// [추가 수정] 다이브 애니메이션도 클라이언트들에게 방송(Multicast)
			if (BossData && BossData->MeteorMontage)
			{
				Multicast_PlayBossMontage(BossData->MeteorMontage, FName("Dive"));
			}

		}, 3.0f, false);

	return (Duration > 0.0f) ? Duration : 5.0f;
}

float AStage3Boss::Execute_PlatformBreak()
{
	if (!PlatformManager || !BossData) return 2.0f;

	// [수정] 새로운 5가지 패턴 중 랜덤 선택 (Enum: 0 ~ 4)
	EPlatformPattern Pat = (EPlatformPattern)FMath::RandRange(0, 4);
	TArray<ABossPlatform*> Targets = PlatformManager->GetPlatformsForPattern(Pat);

	// 경고 표시
	for (ABossPlatform* P : Targets)
	{
		if (P) P->SetWarning(true);
	}

	// 점프 몽타주
	float Duration = 0.0f;
	if (BossData->JumpAttackMontage)
	{
		Duration = PlayAnimMontage(BossData->JumpAttackMontage);
	}

	// 착지 타이밍(2.85초)에 도미노 하강 시작
	// (기존의 순차 하강 로직 유지)
	FTimerHandle LandTimer;
	GetWorldTimerManager().SetTimer(LandTimer, [this, Targets]()
		{
			ProcessDominoDrop(Targets, 0);
		}, 2.85f, false);

	// 자막 출력
	Multicast_ShowBossSubtitle(FText::FromString(TEXT("보스가 발판을 무너뜨립니다!")), 3.0f);

	return (Duration > 0.0f) ? Duration : 3.0f;
}

void AStage3Boss::ProcessDominoDrop(const TArray<ABossPlatform*>& Targets, int32 Index)
{
	if (!Targets.IsValidIndex(Index)) return;

	if (Targets[Index])
	{
		Targets[Index]->MoveDown(false);
	}

	// 0.1초 뒤 다음 발판 호출 (재귀)
	FTimerHandle NextTimer;
	GetWorldTimerManager().SetTimer(NextTimer, [this, Targets, Index]()
		{
			ProcessDominoDrop(Targets, Index + 1);
		}, 0.1f, false);
}

void AStage3Boss::FindNearestPlayer()
{
	AActor* Nearest = nullptr;
	float MinDist = FLT_MAX;

	TArray<AActor*> Players;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), Players);

	for (AActor* P : Players)
	{
		if (P == this) continue;
		float D = GetDistanceTo(P);
		if (D < MinDist)
		{
			MinDist = D;
			Nearest = P;
		}
	}
	LaserTargetActor = Nearest;
}

// ==============================================================================
// [UI Implementation] Stage 1 Boss와 동일한 UI 로직 복사
// ==============================================================================

void AStage3Boss::Multicast_ShowBossSubtitle_Implementation(const FText& Message, float Duration)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->IsLocalPlayerController() || !BossSubtitleWidgetClass) return;

	UUserWidget* SubtitleWidget = CreateWidget<UUserWidget>(PC, BossSubtitleWidgetClass);
	if (SubtitleWidget)
	{
		SubtitleWidget->AddToViewport();

		UFunction* ShowFunc = SubtitleWidget->FindFunction(FName("ShowSubtitle"));
		if (ShowFunc)
		{
			struct { FText TextParams; } Params;
			Params.TextParams = Message;
			SubtitleWidget->ProcessEvent(ShowFunc, &Params);
		}

		FTimerHandle RemoveTimer;
		GetWorldTimerManager().SetTimer(RemoveTimer, [SubtitleWidget]()
			{
				if (IsValid(SubtitleWidget)) SubtitleWidget->RemoveFromParent();
			}, Duration, false);
	}
}

void AStage3Boss::Multicast_ShowBossHP_Implementation(float MaxHP)
{
	if (!BossHPWidgetClass || ActiveBossHPWidget) return;

	APlayerController* LocalPC = nullptr;
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PC = Iterator->Get();
		if (PC && PC->IsLocalController())
		{
			LocalPC = PC;
			break;
		}
	}

	if (!LocalPC) return;

	ActiveBossHPWidget = CreateWidget<UUserWidget>(LocalPC, BossHPWidgetClass);
	if (ActiveBossHPWidget)
	{
		ActiveBossHPWidget->AddToViewport();

		if (BossData && BossData->BossIcon)
		{
			UFunction* InitFunc = ActiveBossHPWidget->FindFunction(FName("InitBossUI"));
			if (InitFunc)
			{
				struct { UTexture2D* IconParam; } InitParams;
				InitParams.IconParam = BossData->BossIcon;
				ActiveBossHPWidget->ProcessEvent(InitFunc, &InitParams);
			}
		}

		UFunction* NameFunc = ActiveBossHPWidget->FindFunction(FName("SetBossName"));
		if (NameFunc)
		{
			struct { FText NameParam; } NameStruct;
			NameStruct.NameParam = BossData->BossName;
			ActiveBossHPWidget->ProcessEvent(NameFunc, &NameStruct);
		}

		UFunction* UpdateFunc = ActiveBossHPWidget->FindFunction(FName("UpdateHP"));
		if (UpdateFunc)
		{
			struct { double Current; double Max; } Params;
			Params.Current = (double)MaxHP;
			Params.Max = (double)MaxHP;
			ActiveBossHPWidget->ProcessEvent(UpdateFunc, &Params);
		}
	}
}

void AStage3Boss::Multicast_UpdateBossHP_Implementation(float CurrentHP, float MaxHP)
{
	if (ActiveBossHPWidget)
	{
		UFunction* UpdateFunc = ActiveBossHPWidget->FindFunction(FName("UpdateHP"));
		if (UpdateFunc)
		{
			struct { double Current; double Max; } Params;
			Params.Current = (double)FMath::RoundToInt(CurrentHP);
			Params.Max = (double)FMath::RoundToInt(MaxHP);
			ActiveBossHPWidget->ProcessEvent(UpdateFunc, &Params);
		}
	}
}

void AStage3Boss::Multicast_HideBossHP_Implementation()
{
	if (ActiveBossHPWidget)
	{
		ActiveBossHPWidget->RemoveFromParent();
		ActiveBossHPWidget = nullptr;
	}
}

// ---------------------------------------------------------
// [추가] Multicast 구현부: 클라이언트 화면에 시각적 요소 렌더링
// ---------------------------------------------------------
void AStage3Boss::Multicast_PlayBossMontage_Implementation(UAnimMontage* MontageToPlay, FName SectionName)
{
	if (MontageToPlay)
	{
		// 서버와 모든 클라이언트에서 몽타주가 실행됩니다.
		PlayAnimMontage(MontageToPlay, 1.0f, SectionName);
	}
}

void AStage3Boss::Multicast_SpawnMeteorFX_Implementation(FVector SpawnLocation)
{
	if (MeteorExplosionFX)
	{
		// 서버와 모든 클라이언트에서 파티클이 터집니다.
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), MeteorExplosionFX, SpawnLocation, FRotator::ZeroRotator, FVector(1.0f), true);
	}
}