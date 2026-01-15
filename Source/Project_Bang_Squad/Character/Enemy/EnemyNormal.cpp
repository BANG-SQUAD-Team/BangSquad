#include "EnemyNormal.h"

#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h" // DrawDebugBox 사용을 위해 필요
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h" // [필수] 플레이어 구분 및 IsDead 확인용

AEnemyNormal::AEnemyNormal()
{
	// [중요] Tick을 켜야 디버그 박스가 그려짐
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// 1. 무기 충돌 박스 생성
	WeaponCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("WeaponCollisionBox"));

	WeaponCollisionBox->SetupAttachment(GetMesh(), TEXT("weapon_root_R"));
	WeaponCollisionBox->SetGenerateOverlapEvents(true);

	// 2. 평소에는 꺼둠 (NoCollision)
	WeaponCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponCollisionBox->SetCollisionObjectType(ECC_WorldDynamic);
	WeaponCollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	WeaponCollisionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap); // 캐릭터랑만 충돌 체크
	WeaponCollisionBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	// 3. 충돌 이벤트 연결
	WeaponCollisionBox->OnComponentBeginOverlap.AddDynamic(this, &AEnemyNormal::OnWeaponOverlap);
}

void AEnemyNormal::BeginPlay()
{
	Super::BeginPlay();

	AcquireTarget();

	if (TargetPawn.IsValid())
	{
		StartChase(TargetPawn.Get());
	}
}

void AEnemyNormal::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// [디버그] 박스 켜져 있을 때만 초록색 박스 그리기
	if (WeaponCollisionBox && WeaponCollisionBox->GetCollisionEnabled() == ECollisionEnabled::QueryOnly)
	{
		DrawDebugBox(
			GetWorld(),
			WeaponCollisionBox->GetComponentLocation(),
			WeaponCollisionBox->GetScaledBoxExtent(),
			WeaponCollisionBox->GetComponentQuat(),
			FColor::Green,
			false,
			-1.0f,
			0,
			2.0f
		);
	}
}

void AEnemyNormal::AcquireTarget()
{
	// [수정] 시작할 때도 가장 가까운 살아있는 플레이어를 찾도록 변경
	APawn* NearestPawn = FindNearestLivingPlayer();
	TargetPawn = NearestPawn;
}

// [추가] 가장 가까운 살아있는 플레이어 찾기 구현
APawn* AEnemyNormal::FindNearestLivingPlayer()
{
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABaseCharacter::StaticClass(), FoundActors);

	APawn* BestPawn = nullptr;
	float MinDistanceSq = FLT_MAX;
	FVector MyLoc = GetActorLocation();

	for (AActor* Actor : FoundActors)
	{
		ABaseCharacter* Player = Cast<ABaseCharacter>(Actor);
		// 죽지 않은 플레이어만 후보로 등록
		if (Player && !Player->IsDead())
		{
			float DistSq = FVector::DistSquared(MyLoc, Player->GetActorLocation());
			if (DistSq < MinDistanceSq)
			{
				MinDistanceSq = DistSq;
				BestPawn = Player;
			}
		}
	}

	return BestPawn;
}

void AEnemyNormal::StartChase(APawn* NewTarget)
{
	if (!NewTarget) return;

	TargetPawn = NewTarget;

	UpdateMoveTo();

	GetWorldTimerManager().ClearTimer(RepathTimer);
	GetWorldTimerManager().SetTimer(
		RepathTimer,
		this,
		&AEnemyNormal::UpdateMoveTo,
		RepathInterval,
		true
	);
}

void AEnemyNormal::StopChase()
{
	GetWorldTimerManager().ClearTimer(RepathTimer);

	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->StopMovement();
		// [중요] 시체나 이전 타겟을 계속 바라보는 것을 방지하기 위해 포커스 해제
		AIC->ClearFocus(EAIFocusPriority::Gameplay);
	}

	TargetPawn = nullptr;
}

void AEnemyNormal::UpdateMoveTo()
{
	// 1. 나 자신이 죽었으면 중단
	if (IsDead()) return;

	APawn* TP = TargetPawn.Get();

	// 2. 타겟이 없거나 유효하지 않으면 -> 새 타겟 찾기 시도
	if (!TP || !IsValid(TP))
	{
		APawn* NewTarget = FindNearestLivingPlayer();
		if (NewTarget) StartChase(NewTarget);
		else StopChase();
		return;
	}

	// =========================================================
	// [핵심 수정] 타겟 검증 로직 (팀킬 방지 & 시체 추격 방지 & 환승)
	// =========================================================
	ABaseCharacter* PlayerTarget = Cast<ABaseCharacter>(TP);
	if (PlayerTarget)
	{
		// 플레이어인데 이미 죽었다면?
		if (PlayerTarget->IsDead())
		{
			// [중요] 즉시 멈추는 게 아니라, 살아있는 다른 플레이어를 찾는다!
			APawn* NewTarget = FindNearestLivingPlayer();
			if (NewTarget)
			{
				StartChase(NewTarget); // 환승
			}
			else
			{
				StopChase(); // 다 죽었으면 멈춤
			}
			return;
		}
	}
	else
	{
		// 플레이어 클래스가 아님 (오인 사격 방지) -> 진짜 플레이어 찾기
		APawn* NewTarget = FindNearestLivingPlayer();
		if (NewTarget) StartChase(NewTarget);
		else StopChase();
		return;
	}
	// =========================================================

	const float Dist = FVector::Dist(GetActorLocation(), TP->GetActorLocation());
	if (Dist > StopChaseDistance)
	{
		// 거리가 너무 멀어졌을 때도 다시 가까운 적을 찾는 게 좋음 (선택 사항)
		APawn* NewTarget = FindNearestLivingPlayer();
		if (NewTarget) StartChase(NewTarget);
		else StopChase();
		return;
	}

	if (bIsAttacking)
	{
		return;
	}

	if (IsInAttackRange())
	{
		if (AAIController* AIC = Cast<AAIController>(GetController()))
		{
			AIC->StopMovement();
		}

		Server_TryAttack();
		return;
	}

	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->MoveToActor(TP, AcceptanceRadius, true, true, true, 0, true);
	}
}

bool AEnemyNormal::IsInAttackRange() const
{
	APawn* TP = TargetPawn.Get();
	if (!TP) return false;

	return FVector::Dist(GetActorLocation(), TP->GetActorLocation()) <= AttackRange;
}

void AEnemyNormal::Server_TryAttack_Implementation()
{
	if (IsDead()) return;
	if (!HasAuthority()) return;

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastAttackTime < AttackCooldown) return;
	if (!IsInAttackRange()) return;

	TArray<int32> ValidIndices;
	ValidIndices.Reserve(AttackConfigs.Num());

	for (int32 i = 0; i < AttackConfigs.Num(); ++i)
	{
		if (AttackConfigs[i].Montage != nullptr)
		{
			ValidIndices.Add(i);
		}
	}

	if (ValidIndices.Num() == 0) return;

	const int32 Pick = ValidIndices[FMath::RandRange(0, ValidIndices.Num() - 1)];
	const FEnemyAttackData& SelectedAttack = AttackConfigs[Pick];

	LastAttackTime = Now;
	bIsAttacking = true;

	Multicast_PlayAttackMontage(Pick);

	// 선딜 타이머
	GetWorldTimerManager().SetTimer(
		CollisionEnableTimer,
		this,
		&AEnemyNormal::EnableWeaponCollision,
		SelectedAttack.HitDelay,
		false
	);

	// 지속시간 타이머
	float DisableTime = SelectedAttack.HitDelay + SelectedAttack.HitDuration;
	GetWorldTimerManager().SetTimer(
		CollisionDisableTimer,
		this,
		&AEnemyNormal::DisableWeaponCollision,
		DisableTime,
		false
	);

	// 전체 종료 타이머
	float Duration = 0.7f;
	if (SelectedAttack.Montage)
	{
		Duration = SelectedAttack.Montage->GetPlayLength();
	}

	GetWorldTimerManager().ClearTimer(AttackEndTimer);
	GetWorldTimerManager().SetTimer(AttackEndTimer, this, &AEnemyNormal::EndAttack, Duration, false);
}

void AEnemyNormal::Multicast_PlayAttackMontage_Implementation(int32 MontageIndex)
{
	if (!AttackConfigs.IsValidIndex(MontageIndex)) return;

	UAnimMontage* MontageToPlay = AttackConfigs[MontageIndex].Montage;
	if (!MontageToPlay) return;

	PlayAnimMontage(MontageToPlay);
}

void AEnemyNormal::EnableWeaponCollision()
{
	// 공격 시작 시 명단 초기화 (중복 피격 방지)
	HitVictims.Empty();

	if (WeaponCollisionBox)
	{
		WeaponCollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
}

void AEnemyNormal::DisableWeaponCollision()
{
	if (WeaponCollisionBox)
	{
		WeaponCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AEnemyNormal::OnWeaponOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
                                   UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
                                   bool bFromSweep, const FHitResult& SweepResult)
{
    if (!HasAuthority()) return;
    if (OtherActor == nullptr || OtherActor == this || OtherActor == GetOwner()) return;
    if (HitVictims.Contains(OtherActor)) return;
    
    if (OtherActor->IsA(ABaseCharacter::StaticClass()))
    {
        // 1. 칼이 직접 방패를 때림
        if (OtherComp && OtherComp->GetName().Contains(TEXT("Shield")))
        {
             UGameplayStatics::ApplyDamage(OtherActor, AttackDamage, GetController(), this, UDamageType::StaticClass());
             HitVictims.Add(OtherActor);
             return;
        }

        // 2. 레이저 검사
        FHitResult HitResult;
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(this); 

        FVector Start = GetActorLocation();
        FVector End = OtherActor->GetActorLocation();

        // [디버그] 빨간 선 그리기
        DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 1.0f, 0, 2.0f);

        bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params);

        if (bHit)
        {
            if (HitResult.GetComponent() && HitResult.GetComponent()->GetName().Contains(TEXT("Shield")))
            {
                AActor* ShieldOwner = HitResult.GetActor();

                // 주인 확인
                if (ShieldOwner == OtherActor || ShieldOwner->GetOwner() == OtherActor)
                {
                    // PASS -> 데미지 적용 (방패 깎임)
                }
                else
                {
                    // BLOCKED -> 리턴 (친구 보호)
                    return; 
                }
            }
        }

        // 데미지 적용
        UGameplayStatics::ApplyDamage(
            OtherActor,
            AttackDamage,
            GetController(),
            this,
            UDamageType::StaticClass()
        );

        HitVictims.Add(OtherActor);
    }
}

void AEnemyNormal::EndAttack()
{
	bIsAttacking = false;
	DisableWeaponCollision();
}

void AEnemyNormal::OnDeathStarted()
{
	Super::OnDeathStarted();

	GetWorldTimerManager().ClearTimer(RepathTimer);
	GetWorldTimerManager().ClearTimer(AttackEndTimer);
	GetWorldTimerManager().ClearTimer(CollisionEnableTimer);
	GetWorldTimerManager().ClearTimer(CollisionDisableTimer);

	StopChase();
	bIsAttacking = false;
	DisableWeaponCollision();
}