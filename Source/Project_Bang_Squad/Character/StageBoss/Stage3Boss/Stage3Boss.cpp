// Source/Project_Bang_Squad/Character/StageBoss/Stage3Boss.cpp

#include "Project_Bang_Squad/Character/StageBoss/Stage3Boss/Stage3Boss.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyBossData.h"
#include "Project_Bang_Squad/Character/StageBoss/Stage3Boss/PlatformManager.h"
#include "Project_Bang_Squad/Character/StageBoss/Stage3Boss/BossPlatform.h"
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
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
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, 2.2f));
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
	// 1. 타겟 선정
	FindNearestPlayer();
	bIsFiringLaser = true;
	
	Multicast_ShowBossSubtitle(FText::FromString(TEXT("보스가 화염 레이저를 조준합니다! 레이저를 피해 도망치세요!!")), 3.0f);

	// 2. 몽타주 재생
	if (BossData && BossData->LaserMontage)
	{
		Multicast_PlayBossMontage(BossData->LaserMontage);
	}

	// 3. 데미지 타이머 시작 (0.25초 간격)
	GetWorldTimerManager().SetTimer(LaserDamageTimer, this, &AStage3Boss::ApplyLaserDamage, 1.0f, true, 1.66f);

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
	// 1. 🚨 소켓 이름 유저님이 원래 쓰시던 걸로 다시 바꿔주세요!! (제가 멋대로 쓴 Spine_02 지우세요)
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
	// 🚨 [바닥 관통 & BoxExtent 적용] 여기서부터 덮어씌우시면 됩니다.
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
	if (!PlatformManager || !BossData) return 2.0f;
	
	Multicast_ShowBossSubtitle(FText::FromString(TEXT("하늘에서 거대한 운석이 떨어집니다! 운석을 피해 도망치세요!")), 3.0f);

	// 1. 몽타주 재생 (서버에서 실행 시 Multicast_PlayBossMontage를 통해 동기화)
	Multicast_PlayBossMontage(BossData->MeteorMontage, FName("Cast"));

	// 2. 타겟 발판 선정 (매니저가 플레이어 위치를 고려해 16칸 중 선정)
	TArray<ABossPlatform*> Targets = PlatformManager->GetPlatformsForMeteor();

	// 3. 0.5초 뒤 경고(붉은 장판) 표시
	FTimerHandle WarningTimer;
	GetWorldTimerManager().SetTimer(WarningTimer, [Targets]()
		{
			for (ABossPlatform* P : Targets) if (P) P->SetWarning(true);
		}, 0.5f, false);

	// 4. [핵심 추가] 1초 뒤 메테오 투사체 실제 스폰 (하늘에서 떨어지는 비주얼)
	// 왜 이렇게 짰는지: 단순히 데미지만 주는 게 아니라, 유저가 하늘을 봤을 때 유성이 떨어지는 '공포감'을 주기 위해 실제 액터를 스폰합니다.
	FTimerHandle SpawnTimer;
	GetWorldTimerManager().SetTimer(SpawnTimer, [this, Targets]()
		{
			// 인덱스(i)를 활용하기 위해 range-based for 대신 일반 for문 사용
			for (int32 i = 0; i < Targets.Num(); ++i)
			{
				ABossPlatform* P = Targets[i];
				if (P && BossData && BossData->MeteorProjectileClass)
				{
					// 각 메테오마다 i * 0.2초의 차이를 둡니다. (0초, 0.2초, 0.4초...)
					float IndividualDelay = i * 0.2f;

					FTimerHandle EachMeteorTimer;
					GetWorldTimerManager().SetTimer(EachMeteorTimer, [this, P]()
						{
							if (!P || !BossData) return;

							// 1. 스폰 위치 계산 (높이 상향 및 사선 궤적 적용)
							FVector TargetLoc = P->GetActorLocation();
							FVector SpawnLoc = TargetLoc + FVector(0, 0, 5000.0f) + (GetActorForwardVector() * -1500.0f);
							FRotator SpawnRot = (TargetLoc - SpawnLoc).Rotation();

							FActorSpawnParameters Params;
							Params.Instigator = this;
							Params.Owner = this;

							// 2. 메테오 스폰
							GetWorld()->SpawnActor<AActor>(BossData->MeteorProjectileClass, SpawnLoc, SpawnRot, Params);

						}, IndividualDelay, false);
				}
			}
		}, 1.0f, false);

	// 5. 3초 뒤 지면 충돌 및 발판 하강 처리
	FTimerHandle ImpactTimer;
	GetWorldTimerManager().SetTimer(ImpactTimer, [this, Targets]()
		{
			for (ABossPlatform* P : Targets)
			{
				if (P)
				{
					// 폭발 이펙트 재생 (Multicast)
					Multicast_SpawnMeteorFX(P->GetActorLocation());

					// 데미지 판정 및 발판 하강 명령
					P->MoveDown(false);
				}
			}
		}, 3.0f, false);

	return 5.0f;
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
		ABaseCharacter* BaseChar = Cast<ABaseCharacter>(P);
		if (BaseChar && BaseChar->IsDead()) continue;
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