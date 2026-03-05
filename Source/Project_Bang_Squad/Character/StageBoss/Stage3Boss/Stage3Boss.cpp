// Source/Project_Bang_Squad/Character/StageBoss/Stage3Boss.cpp

#include "Project_Bang_Squad/Character/StageBoss/Stage3Boss/Stage3Boss.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBossData.h"
#include "Project_Bang_Squad/Character/StageBoss/Stage3Boss/PlatformManager.h"
#include "Project_Bang_Squad/Character/StageBoss/Stage3Boss/BossPlatform.h"
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Project_Bang_Squad/Character/StageBoss/Stage3Boss/Stage3BossAIController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/DamageEvents.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Blueprint/UserWidget.h"

#include "Project_Bang_Squad/Core/TrueDamageType.h"

AStage3Boss::AStage3Boss()
{
	// 보스 덩치 설정
	GetMesh()->SetRelativeScale3D(FVector(2.0f));
	PrimaryActorTick.bCanEverTick = true;
	GetCharacterMovement()->MaxWalkSpeed = 750.0f;
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
				HC->OnHealthChanged.AddDynamic(this, &AStage3Boss::OnHealthChanged);

				// [수정됨] _Implementation 직접 호출 제거 및 서버에서 Multicast RPC 정식 호출
				Multicast_ShowBossHP(HC->MaxHealth);
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
	FTimerHandle LocalUITimer;
	TWeakObjectPtr<AStage3Boss> WeakThis = this;

	// 1초 뒤에 UI를 띄우도록 설정하여, 클라이언트가 맵 로딩과 플레이어 빙의를 마칠 시간을 벌어줌
	GetWorldTimerManager().SetTimer(LocalUITimer, [WeakThis]()
	{
	   if (WeakThis.IsValid())
	   {
		  float InitialMaxHP = 100.0f;
		  if (WeakThis->BossData) 
		  {
			  InitialMaxHP = WeakThis->BossData->MaxHealth;
		  }
		  else if (UHealthComponent* HC = WeakThis->FindComponentByClass<UHealthComponent>()) 
		  {
			  InitialMaxHP = HC->MaxHealth;
		  }

		  // 직접 내 화면에 UI 생성 (서버/클라 모두 각자 실행됨)
		  WeakThis->Multicast_ShowBossHP_Implementation(InitialMaxHP);
	   }
	}, 1.0f, false);

	if (HasAuthority())
	{
		CheckAndHandleFalling();
	}
}

void AStage3Boss::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 왜 이렇게 짰는지: [기획 의도 보존 & 권한 분리] 
	// 레이저를 플레이어가 피할 수 있도록 느리고 부드럽게 회전시킵니다.
	// 트랜스폼(회전) 변경은 서버에서만 수행해야 엔진의 Movement Replication을 통해 클라이언트가 덜덜 떨리지 않고 부드럽게 동기화됩니다.
	if (bIsFiringLaser && HasAuthority())
	{
		// 타겟이 없거나, 언리얼5 표준 유효성 검사 실패 시, 혹은 타겟이 죽었을 때 리타겟팅
		ABaseCharacter* TargetChar = Cast<ABaseCharacter>(LaserTargetActor);
		if (!IsValid(LaserTargetActor) || (IsValid(TargetChar) && TargetChar->IsDead()))
		{
			FindNearestPlayer();
		}

		if (IsValid(LaserTargetActor))
		{
			FVector Dir = LaserTargetActor->GetActorLocation() - GetActorLocation();
			Dir.Z = 0.0f; // 높이는 무시하고 Z축(Yaw) 회전만 적용
			FRotator TargetRot = Dir.Rotation();

			// 기획 의도: 유저가 피할 수 있는 속도 (InterpSpeed 2.2f)
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, 2.2f));
		}
	}
	if (HasAuthority())
	{
		// =========================================================================
		// 🟢 [디버그용] 보스의 현재 Z 높이를 화면 좌측 상단에 노란색으로 찍어봅니다!
		// =========================================================================
		GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Yellow, FString::Printf(TEXT("보스 현재 높이(Z): %f / 임계점: %f"), GetActorLocation().Z, FallThresholdZ));

		CheckAndHandleFalling();
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
			
			Multicast_ShowBossSubtitle(FText::FromString(TEXT("보스를 성공적으로 처치했습니다! BangSquad의 승리입니다!!")), 5.0f);
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
	// [안전성] 서버 권한 한 번 더 체크
	if (!HasAuthority()) return 0.0f;

	FindNearestPlayer();
	bIsFiringLaser = true;

	Multicast_ShowBossSubtitle(FText::FromString(TEXT("보스가 화염 레이저를 조준합니다! 레이저를 피해 도망치세요!!")), 3.0f);

	if (BossData && BossData->LaserMontage)
	{
		Multicast_PlayBossMontage(BossData->LaserMontage);
	}

	GetWorldTimerManager().SetTimer(LaserDamageTimer, this, &AStage3Boss::ApplyLaserDamage, 1.0f, true, 1.66f);

	// [수정됨] TWeakObjectPtr를 사용한 안전한 람다 캡처 (서버 크래시 방지)
	TWeakObjectPtr<AStage3Boss> WeakThis(this);
	FTimerHandle EndHandle;
	GetWorldTimerManager().SetTimer(EndHandle, [WeakThis]()
		{
			if (!WeakThis.IsValid()) return; // 보스가 죽거나 소멸했으면 실행 취소

			WeakThis->bIsFiringLaser = false;
			WeakThis->GetWorldTimerManager().ClearTimer(WeakThis->LaserDamageTimer);

			if (WeakThis->GetMesh() && WeakThis->GetMesh()->GetAnimInstance())
			{
				WeakThis->GetMesh()->GetAnimInstance()->Montage_JumpToSection(FName("End"), WeakThis->BossData->LaserMontage);
			}
		}, 3.0f, false);

	return 4.0f;
}

void AStage3Boss::ApplyLaserDamage()
{
	FName Ray = FName("Ray");
	FVector Start = GetMesh()->GetSocketLocation(Ray);

	FVector ForwardDir = GetActorForwardVector();

	float LaserRange = 5000.0f;
	FVector End = Start + (ForwardDir * LaserRange);

	TArray<FHitResult> Hits;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.bTraceComplex = false;

	FQuat BoxRotation = GetActorRotation().Quaternion();
	FVector BoxExtent = FVector(50.0f, 50.0f, 300.0f);

	// =====================================================================
	// [바닥 관통 & BoxExtent 적용] 여기서부터 덮어씌우시면 됩니다.
	// =====================================================================
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn); // 오직 플레이어(Pawn)만 감지해서 바닥 긁힘 방지

	// ByChannel이 아니라 ByObjectType 입니다!
	bool bHit = GetWorld()->SweepMultiByObjectType(
		Hits,
		Start,
		End,
		BoxRotation,
		ObjectParams,
		FCollisionShape::MakeBox(BoxExtent), //  50,50,50 지우고 드디어 BoxExtent가 들어갑니다!
		Params
	);
	// =====================================================================

	DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 0.25f, 0, 3.0f);

	if (bHit && Hits.Num() > 0)
	{
		DrawDebugBox(GetWorld(), Hits[0].Location, BoxExtent, BoxRotation, FColor::Green, false, 0.25f);
	}

	if (bHit)
	{
		for (auto& Hit : Hits)
		{
			if (Hit.GetActor() && Hit.GetActor()->IsA(ACharacter::StaticClass()))
			{
				UGameplayStatics::ApplyDamage(Hit.GetActor(), 25.0f, GetController(), this, UDamageType::StaticClass());
			}
		}
	}
}

float AStage3Boss::Execute_Meteor()
{
	// 왜 이렇게 짰는지: [권한 분리] 스킬의 시작과 타이머 관리는 무조건 서버에서 주도합니다.
	if (!HasAuthority() || !PlatformManager || !BossData) return 2.0f;

	Multicast_ShowBossSubtitle(FText::FromString(TEXT("하늘에서 거대한 운석이 떨어집니다! 운석을 피해 도망치세요!")), 3.0f);
	Multicast_PlayBossMontage(BossData->MeteorMontage, FName("Cast"));

	TArray<ABossPlatform*> Targets = PlatformManager->GetPlatformsForMeteor();

	FTimerHandle WarningTimer;
	GetWorldTimerManager().SetTimer(WarningTimer, [Targets]()
		{
			for (ABossPlatform* P : Targets) if (IsValid(P)) P->SetWarning(true);
		}, 0.5f, false);

	TWeakObjectPtr<AStage3Boss> WeakThis(this);
	FTimerHandle SpawnTimer;
	GetWorldTimerManager().SetTimer(SpawnTimer, [WeakThis, Targets]()
		{
			if (!WeakThis.IsValid() || !WeakThis->BossData) return;

			for (int32 i = 0; i < Targets.Num(); ++i)
			{
				ABossPlatform* P = Targets[i];
				if (IsValid(P) && WeakThis->BossData->MeteorProjectileClass)
				{
					float IndividualDelay = i * 0.2f;

					FTimerHandle EachMeteorTimer;
					WeakThis->GetWorldTimerManager().SetTimer(EachMeteorTimer, [WeakThis, P]()
						{
							if (!WeakThis.IsValid() || !IsValid(P)) return;

							FVector TargetLoc = P->GetActorLocation();

							// ====================================================================
							// 🟢 [수직 낙하로 수정된 부분]
							// ====================================================================

							// [삭제된 기존 코드 (보스 등 뒤에서 대각선으로 날아오던 로직)]
							// FVector SpawnLoc = TargetLoc + FVector(0, 0, 6000.0f) + (WeakThis->GetActorForwardVector() * -1000.0f);

							// [새로운 코드 (완벽한 수직 스폰)]
							// 왜 이렇게 짰는지: 보스의 방향 벡터를 곱하던 부분을 아예 지워서 발판 정중앙 위쪽에 스폰시킵니다.
							FVector SpawnLoc = TargetLoc + FVector(0.0f, 0.0f, 6000.0f);

							// 위치가 정확히 수직이므로, Rotation 값은 바닥을 향해 수직(-90도)으로 내리꽂히는 각도가 됩니다.
							FRotator SpawnRot = (TargetLoc - SpawnLoc).Rotation();

							FActorSpawnParameters Params;
							Params.Instigator = WeakThis.Get();
							Params.Owner = WeakThis.Get();
							Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

							WeakThis->GetWorld()->SpawnActor<AActor>(WeakThis->BossData->MeteorProjectileClass, SpawnLoc, SpawnRot, Params);

							// 🟢 [수정 2] 스폰 높이가 높아졌으므로, 땅에 닿기까지의 비행 시간(TravelTime)도 1.5 -> 2.0초 정도로 늘려줍니다.
							// (만약 시각적으로 투사체가 땅에 닿기 전에 이펙트가 터진다면 이 시간을 조금 늘리고, 너무 늦게 터진다면 줄여주세요!)
							float TravelTime = 2.0f;

							FTimerHandle ImpactTimer;
							WeakThis->GetWorldTimerManager().SetTimer(ImpactTimer, [WeakThis, P]()
								{
									if (WeakThis.IsValid() && IsValid(P))
									{
										// 🟢 [수정 3] 발판 하강(P->MoveDown) 코드 완전 삭제 유지!
										// 오직 비주얼(폭발 이펙트)과 장판의 데미지 판정만 작동하게 됩니다.
										WeakThis->Multicast_SpawnMeteorFX(P->GetActorLocation());
									}
								}, TravelTime, false);

						}, IndividualDelay, false);
				}
			}
		}, 1.0f, false);

	return 5.0f;
}

float AStage3Boss::Execute_PlatformBreak()
{
	if (!HasAuthority() || !PlatformManager || !BossData) return 2.0f;

	EPlatformPattern Pat = (EPlatformPattern)FMath::RandRange(0, 4);
	TArray<ABossPlatform*> Targets = PlatformManager->GetPlatformsForPattern(Pat);

	for (ABossPlatform* P : Targets)
	{
		if (IsValid(P)) P->SetWarning(true);
	}

	float Duration = 0.0f;
	if (BossData->JumpAttackMontage)
	{
		// [수정됨] 로컬 재생을 지우고 Multicast_ RPC를 사용하여 클라이언트도 점프 몽타주를 보게 변경
		Multicast_PlayBossMontage(BossData->JumpAttackMontage);
		Duration = BossData->JumpAttackMontage->GetPlayLength();
	}

	TWeakObjectPtr<AStage3Boss> WeakThis(this);
	FTimerHandle LandTimer;
	GetWorldTimerManager().SetTimer(LandTimer, [WeakThis, Targets]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->ProcessDominoDrop(Targets, 0);
			}
		}, 2.85f, false);

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
		ABaseCharacter* BaseChar = Cast<ABaseCharacter>(P);

		// [최적화] UE5 권장 사양: IsPendingKillPending() 대신 IsValid() 사용
		if (!IsValid(BaseChar) || BaseChar->IsDead()) continue;

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
// [UI Implementation]  UI 로직 
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

	//  무조건 내 모니터의 주인(0번)을 가져온다
	APlayerController* LocalPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    
	// 혹시라도 로컬 플레이어가 아니면 컷
	if (!LocalPC || !LocalPC->IsLocalPlayerController()) return;

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

void AStage3Boss::DoMeleeHitCheck()
{
	// 권한 분리: 데미지는 무조건 서버에서만 판정합니다!
	if (!HasAuthority()) return;

	// 보스 정면 300 거리에 반경 200짜리 구를 생성하여 충돌 체크
	FVector Start = GetActorLocation() + GetActorForwardVector() * 300.0f;
	FVector End = Start + GetActorForwardVector() * 50.0f; // 살짝 앞으로 밀면서 스윕
	float AttackRadius = 200.0f;

	TArray<FHitResult> HitResults;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this); // 자기 자신 제외

	bool bHit = GetWorld()->SweepMultiByChannel(
		HitResults, Start, End, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeSphere(AttackRadius), Params
	);

	if (bHit)
	{
		for (const FHitResult& Hit : HitResults)
		{
			ACharacter* HitPlayer = Cast<ACharacter>(Hit.GetActor());
			// 멀티플레이 방어코드: 죽어가는 타겟에는 데미지를 주지 않음
			if (IsValid(HitPlayer) && !HitPlayer->IsPendingKillPending())
			{
				UGameplayStatics::ApplyDamage(HitPlayer, 40.0f, GetController(), this, UDamageType::StaticClass());

				// (선택) 출혈 이펙트 등 멀티캐스트 호출 가능
			}
		}
	}
}

void AStage3Boss::CheckAndHandleFalling()
{
	// 보스의 현재 높이가 마지노선보다 아래로 떨어졌다면
	if (GetActorLocation().Z < FallThresholdZ)
	{
		FindNearestPlayer();
		AActor* Target = LaserTargetActor;
		if (!IsValid(Target)) return;

		// 텔레포트 위치 계산 (플레이어 코앞 + 높이 보정)
		FVector TeleportLoc = Target->GetActorLocation() + (Target->GetActorForwardVector() * 200.0f);
		TeleportLoc.Z = Target->GetActorLocation().Z + 150.0f;
		FRotator TeleportRot = (Target->GetActorLocation() - TeleportLoc).Rotation();
		TeleportRot.Pitch = 0.0f;
		TeleportRot.Roll = 0.0f;

		// 순간이동!
		SetActorLocationAndRotation(TeleportLoc, TeleportRot, false, nullptr, ETeleportType::TeleportPhysics);

		if (GetCharacterMovement())
		{
			GetCharacterMovement()->Velocity = FVector::ZeroVector;
		}

		Multicast_ShowBossSubtitle(FText::FromString(TEXT("보스가 등 뒤로 순간이동했습니다!!")), 2.0f);

		// =========================================================================
		// 🟢 [수정됨] 강제 평타 실행 후, AI 컨트롤러의 스케줄을 동기화!
		// =========================================================================
		float AnimDuration = Execute_BasicAttack(); // 몽타주 길이 반환받음

		if (AStage3BossAIController* AIController = Cast<AStage3BossAIController>(GetController()))
		{
			// AI 컨트롤러야, (애니메이션 길이 + 후딜레이 1.5초) 만큼 아무것도 하지 말고 대기해라!
			AIController->ForceActionTimerDelay(AnimDuration + 1.5f);
		}
	}
}