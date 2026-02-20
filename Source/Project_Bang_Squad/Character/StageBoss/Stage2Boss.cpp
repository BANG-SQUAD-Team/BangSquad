// Source/Project_Bang_Squad/Character/StageBoss/Stage2Boss.cpp

#include "Project_Bang_Squad/Character/StageBoss/Stage2Boss.h"
#include "Project_Bang_Squad/Character/StageBoss/Stage2SpiderAIController.h"
#include "Project_Bang_Squad/Character/Enemy/EnemySpawner.h"
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h" // 헤더 추가
#include "DrawDebugHelpers.h"
#include "Project_Bang_Squad/Character/StageBoss/AQTE_Trap.h" // (경로가 폴더 안에 있다면 "Project_Bang_Squad/Character/StageBoss/AQTE_Trap.h" 로 맞춰주세요)
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Kismet/KismetSystemLibrary.h" // SphereTrace용
#include "Project_Bang_Squad/Character/StageBoss/BossSplitPattern.h"





AStage2Boss::AStage2Boss()
{
    AIControllerClass = AStage2SpiderAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
    GetCharacterMovement()->bCanWalkOffLedges = false;
}

void AStage2Boss::BeginPlay()
{
    Super::BeginPlay();

    // [핵심] DataAsset이 있으면, 내 변수에 덮어씌운다 (DataAsset 우선주의)
    if (BossData)
    {
        HealthComponent->SetMaxHealth(BossData->MaxHealth);

        // DataAsset에 설정된 게 있다면 가져오기
        // (EnemyBossData에 해당 변수가 있어야 합니다)
        if (BossData->WebProjectileClass)
            WebProjectileClass = BossData->WebProjectileClass;

        if (BossData->WebShotMontage)
            WebShotMontage = BossData->WebShotMontage;

        if (BossData->QTEAttackMontage)
            SmashMontage = BossData->QTEAttackMontage;
    }
    // [테스트용 임시 코드] 70% 페이즈를 이미 본 것처럼 처리해서 스킵!
    bPhase70Triggered = true;
}

void AStage2Boss::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

float AStage2Boss::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (bIsInvincible) return 0.0f;

    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    // ü�� ������ üũ
    CheckHealthPhase();

    return ActualDamage;
}

void AStage2Boss::CheckHealthPhase()
{
    if (!HealthComponent) return;

    
    float MaxHP = HealthComponent->GetMaxHealth();
    float CurHP = HealthComponent->GetHealth();
    float HPRatio = (MaxHP > 0.0f) ? (CurHP / MaxHP) : 0.0f;

    // 70%
    if (!bPhase70Triggered && HPRatio <= 0.7f)
    {
        bPhase70Triggered = true;
        bIsInvincible = true;

        if (auto* AI = Cast<AStage2SpiderAIController>(GetController()))
            AI->StartPhasePattern();

        // StartSpawning -> SetSpawnerActive
        if (MinionSpawner)
            MinionSpawner->SetSpawnerActive(true);

        FTimerHandle CheckHandle;
        GetWorld()->GetTimerManager().SetTimer(CheckHandle, this, &AStage2Boss::CheckMinionsStatus, 1.0f, true);

        UE_LOG(LogTemp, Warning, TEXT("Boss Phase 1 (70 Percent) Started!"));
    }
    // ==========================================================
    // 2. [50% 구간] 인원 분배 패턴 기믹 (신규)
    // ==========================================================
    else if (!bPhase50Triggered && HPRatio <= 0.5f && HPRatio > 0.3f)
    {
        bPhase50Triggered = true;
        bIsInvincible = true;

        // AI 정지 (PhaseWait 대기 상태로 전환)
        if (auto* AI = Cast<AStage2SpiderAIController>(GetController()))
            AI->StartPhasePattern();

        // 몽타주 재생을 통해 기믹 시작을 알림 (클라이언트 동기화)
        Multicast_PlayPhase50Montage();

        UE_LOG(LogTemp, Warning, TEXT("Boss Phase 2 (50 Percent) Started - Split Pattern Mechanic!"));
    }

    // 30%
    else if (!bPhase30Triggered && HPRatio <= 0.3f)
    {
        bPhase30Triggered = true;
        bIsInvincible = true;

        if (auto* AI = Cast<AStage2SpiderAIController>(GetController()))
            AI->StartPhasePattern();

        if (MinionSpawner)
            MinionSpawner->SetSpawnerActive(true);

        FTimerHandle CheckHandle;
        GetWorld()->GetTimerManager().SetTimer(CheckHandle, this, &AStage2Boss::CheckMinionsStatus, 1.0f, true);

        UE_LOG(LogTemp, Warning, TEXT("Boss Phase 2 (30 Percent) Started!"));
    }
}

// -------------------------------------------------------------
// [50% 기믹 관련 함수 구현부]
// -------------------------------------------------------------

void AStage2Boss::Multicast_PlayPhase50Montage_Implementation()
{
    // [원칙 2] 모든 클라이언트에서 몽타주 재생
    if (Phase50Montage)
    {
        PlayAnimMontage(Phase50Montage);
    }
}

void AStage2Boss::SpawnSplitPattern()
{
    if (!HasAuthority()) return;

    if (SplitPatternClass)
    {
        FVector SpawnLoc = GetActorLocation();
        FRotator SpawnRot = FRotator::ZeroRotator;

        ABossSplitPattern* SpawnedPattern = GetWorld()->SpawnActor<ABossSplitPattern>(SplitPatternClass, SpawnLoc, SpawnRot);

        if (SpawnedPattern)
        {
            SpawnedPattern->OnSplitPatternFinished.AddDynamic(this, &AStage2Boss::HandleSplitPatternResult);

            // [핵심] Phase50Montage의 총 재생 길이를 가져옵니다. (몽타주가 없으면 기본 5초)
            float MontageLength = Phase50Montage ? Phase50Montage->GetPlayLength() : 5.0f;

            // 장판에게 "내 몽타주 시간만큼만 켜져 있어라!" 하고 지시합니다.
            SpawnedPattern->ActivatePattern(MontageLength);
        }
    }
}
void AStage2Boss::Multicast_SetBossVisibility_Implementation(bool bIsVisible)
{
    // 몽타주 Notify를 통해 보스 숨김/표시 처리
    GetMesh()->SetVisibility(bIsVisible);
}

void AStage2Boss::HandleSplitPatternResult(bool bIsSuccess)
{
    // [원칙 1] 데미지 판정은 무조건 서버에서만!
    if (!HasAuthority()) return;

    bIsInvincible = false;
    Multicast_SetBossVisibility(true);

    if (auto* AI = Cast<AStage2SpiderAIController>(GetController()))
    {
        AI->EndPhasePattern();
    }

    if (bIsSuccess)
    {
        UE_LOG(LogTemp, Log, TEXT("50%% 인원 분배 패턴 성공!"));
        // TODO: 기획에 따라 보스가 잠시 기절(그로기)하는 로직을 넣어도 좋습니다.
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("50%% 인원 분배 패턴 실패! 전멸기 데미지 적용!"));

        // ==========================================================
        // [실패 패널티: 전멸기 데미지 구현]
        // ==========================================================
        TArray<AActor*> AllPlayers;
        // 맵에 있는 모든 플레이어(ABaseCharacter)를 찾습니다.
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABaseCharacter::StaticClass(), AllPlayers);

        for (AActor* PlayerActor : AllPlayers)
        {
            if (ABaseCharacter* Player = Cast<ABaseCharacter>(PlayerActor))
            {
                if (!Player->IsDead()) // 아직 살아있는 플레이어라면
                {
                    // 무자비한 9999 데미지를 가합니다!
                    UGameplayStatics::ApplyDamage(Player, 9999.0f, GetController(), this, UDamageType::StaticClass());
                }
            }
        }
    }
}




void AStage2Boss::CheckMinionsStatus()
{
    // [�ʿ�] EnemySpawner�� GetCurrentEnemyCount() �Լ��� �ʿ��մϴ�.
    if (MinionSpawner && MinionSpawner->GetCurrentEnemyCount() <= 0)
    {
        bIsInvincible = false;

        if (auto* AI = Cast<AStage2SpiderAIController>(GetController()))
        {
            AI->EndPhasePattern();
        }

        GetWorld()->GetTimerManager().ClearAllTimersForObject(this);

        UE_LOG(LogTemp, Warning, TEXT("Phase Ended! Boss Vulnerable."));
    }
}

// Source/Project_Bang_Squad/Character/StageBoss/Stage2Boss.cpp
#include "Components/CapsuleComponent.h"
// ...

// 1. 패턴 시작: 조준하고 몽타주만 틉니다. (발사는 안 함)
void AStage2Boss::PerformWebShot(AActor* Target)
{
    if (!Target) return;

    // 조준 (타겟 바라보기)
    FVector Direction = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal();
    FRotator LookAtRot = FRotationMatrix::MakeFromX(Direction).Rotator();
    SetActorRotation(FRotator(0, LookAtRot.Yaw, 0));

    // 몽타주 재생
    // (이 몽타주 안에 심어둔 Notify가 발동되면 -> FireWebProjectile()이 실행됩니다)
    if (WebShotMontage)
    {
        Multicast_PlayBossMontage(WebShotMontage);
    }
    else
    {
        // 만약 몽타주가 없으면? 비상용으로 즉시 발사 (테스트용)
        FireWebProjectile();
    }
}

void AStage2Boss::FireWebProjectile()
{
    // [권한 분리] 투사체는 무조건 서버에서만 단 1개 스폰해야 합니다! 
    // (이게 없어서 클라이언트가 만든 가짜 투사체가 평생 남아있던 것입니다)
    if (!HasAuthority()) return;
    // 1. 클래스 체크
    if (!WebProjectileClass)
    {
        UE_LOG(LogTemp, Error, TEXT("!!! [FireWebShot] WebProjectileClass is Missing !!!"));
        return;
    }

    // 2. 위치 계산 (보스 덩치 고려)
    float BossRadius = 0.0f;
    if (GetCapsuleComponent())
    {
        BossRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
    }

    // 반지름 + 100만큼 앞에서 생성 (최대한 안 겹치게)
    FVector SpawnLoc = GetActorLocation() + (GetActorForwardVector() * (BossRadius + 100.0f));
    SpawnLoc.Z += 50.0f;

    FRotator SpawnRot = GetActorRotation();

    // 3. 스폰 파라미터 (충돌 무시 옵션)
    FActorSpawnParameters Params;
    Params.Owner = this;        // 주인은 나(보스)
    Params.Instigator = this;   // 가해자도 나
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn; // 겹쳐도 무조건 생성

    // 4. 진짜 생성
    AActor* Projectile = GetWorld()->SpawnActor<AActor>(WebProjectileClass, SpawnLoc, SpawnRot, Params);

    if (Projectile)
    {
        // [핵심] 보스 자신과의 충돌을 무시하도록 설정
        // 투사체의 충돌 컴포넌트(Sphere나 Box 등)를 찾아서 보스를 무시하게 함
        if (UPrimitiveComponent* Primitive = Projectile->FindComponentByClass<UPrimitiveComponent>())
        {
            Primitive->IgnoreActorWhenMoving(this, true); // "나(this)랑 부딪혀도 멈추지 마"
        }

        // 5. 이동 컴포넌트 강제 설정
        UProjectileMovementComponent* PMC = Projectile->FindComponentByClass<UProjectileMovementComponent>();
        if (PMC)
        {
            // BP 값을 가져와서 확실하게 다시 세팅 (혹시 초기화 타이밍 문제 방지)
            float Speed = (PMC->InitialSpeed > 0) ? PMC->InitialSpeed : 1500.0f;

            PMC->SetVelocityInLocalSpace(FVector(Speed, 0, 0)); // 앞으로 쏴라!
            PMC->UpdateComponentVelocity();
        }

        UE_LOG(LogTemp, Log, TEXT("[FireWebShot] Projectile Fired! (Collision Ignored)"));
    }
}

void AStage2Boss::PerformSmashAttack(AActor* Target)
{
    if (SmashMontage) Multicast_PlayBossMontage(SmashMontage);
}

void AStage2Boss::StartQTEPattern(AActor* Target)
{
    // QTE ���� ����
}

void AStage2Boss::OnQTEResult(bool bSuccess)
{
    if (bSuccess)
    {
        UE_LOG(LogTemp, Log, TEXT("QTE Success!"));
        // �׷α� ���� �� ó��
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("QTE Failed!"));
        // ����� �� ó��
    }
}

float AStage2Boss::PlayMeleeAttackAnim()
{
    if (BossData && BossData->AttackMontages.Num() > 0)
    {
        UAnimMontage* MontageToPlay = BossData->AttackMontages[0];

        // [비주얼 동기화] 모든 클라이언트에서 평타 애니메이션 재생
        Multicast_PlayBossMontage(MontageToPlay);

        // 멀티캐스트는 void 반환이므로, 몽타주 에셋에서 직접 길이를 빼와서 AI에게 전달
        return MontageToPlay->GetPlayLength();
    }
    return 0.0f;
}

void AStage2Boss::ServerRPC_QTEResult_Implementation(APlayerController* Player, bool bSuccess)
{
    // 서버 로그 출력
    UE_LOG(LogTemp, Warning, TEXT("[Server] Received QTE Result from %s : %s"), 
        *Player->GetName(), 
        bSuccess ? TEXT("Success") : TEXT("Fail"));

    // 기존에 만들어두신 OnQTEResult 함수를 호출하여 로직 처리
    OnQTEResult(bSuccess);
}

void AStage2Boss::PerformSmashHitCheck()
{
    if (!HasAuthority()) return;

    FVector StartLoc = GetActorLocation() + (GetActorForwardVector() * SmashForwardOffset);
    StartLoc.Z += SmashZOffset;

    DrawDebugSphere(GetWorld(), StartLoc, SmashRadius, 16, FColor::Red, false, 3.0f);

    // [수정] FHitResult 대신 FOverlapResult 배열을 생성합니다.
    TArray<FOverlapResult> OverlapResults;
    FCollisionShape Sphere = FCollisionShape::MakeSphere(SmashRadius);
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    bool bHit = GetWorld()->OverlapMultiByChannel(
        OverlapResults, StartLoc, FQuat::Identity, ECC_Pawn, Sphere, Params
    );

    if (bHit)
    {
        // [수정] FOverlapResult로 순회합니다.
        for (const FOverlapResult& Overlap : OverlapResults)
        {
            // [수정] Hit.GetActor() 대신 Overlap.GetActor()를 사용합니다.
            if (ABaseCharacter* HitPlayer = Cast<ABaseCharacter>(Overlap.GetActor()))
            {
                if (HitPlayer->IsDead()) continue;
                GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, FString::Printf(TEXT("Smash Hit Player: %s"), *HitPlayer->GetName()));
                UGameplayStatics::ApplyDamage(HitPlayer, 50.0f, GetController(), this, UDamageType::StaticClass());

                if (QTETrapClass)
                {
                    FActorSpawnParameters SpawnParams;
                    SpawnParams.Owner = this;
                    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                    AQTE_Trap* NewTrap = GetWorld()->SpawnActor<AQTE_Trap>(QTETrapClass, HitPlayer->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
                    if (NewTrap) NewTrap->InitializeTrap(HitPlayer, 10);
                }
            }
        }
    }
}


void AStage2Boss::ExecuteFollowUpKnockback()
{
    // [권한 분리] 넉백 물리 처리는 서버에서만 합니다.
    if (!HasAuthority()) return;

    // [수정] ZOffset을 더해서 시작 높이를 조절합니다.
    FVector TraceStart = GetActorLocation();
    TraceStart.Z += FollowUpTraceZOffset;

    // [수정] ForwardOffset 변수를 곱해서 앞으로 뻗어나가는 거리를 조절합니다.
    FVector TraceEnd = TraceStart + (GetActorForwardVector() * FollowUpTraceForwardOffset);

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(this);
    TArray<FHitResult> OutHits;

    // [충돌 동기화] 서버에서 트레이스 실행
    bool bHit = UKismetSystemLibrary::SphereTraceMulti(
        GetWorld(), TraceStart, TraceEnd, FollowUpTraceRadius,
        UEngineTypes::ConvertToTraceType(ECC_Pawn), false, ActorsToIgnore,
        EDrawDebugTrace::ForDuration, OutHits, true
    );

    if (bHit)
    {
        for (const FHitResult& Hit : OutHits)
        {
            if (ACharacter* Target = Cast<ACharacter>(Hit.GetActor()))
            {
                // [해결 1] 넉백을 받기 위해 캐릭터의 이동 모드를 강제로 '공중(Falling)'으로 변경
                if (Target->GetCharacterMovement())
                {
                    Target->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
                }

                // [해결 2] 플레이어 주변에 있는 트랩(AQTE_Trap)을 찾아 강제 파괴
                // (트랩이 플레이어와 겹쳐있거나 자식으로 붙어있을 경우 모두 처리)
                TArray<AActor*> AttachedOrOverlappingTraps;
                Target->GetOverlappingActors(AttachedOrOverlappingTraps, AQTE_Trap::StaticClass());

                for (AActor* TrapActor : AttachedOrOverlappingTraps)
                {
                    if (AQTE_Trap* Trap = Cast<AQTE_Trap>(TrapActor))
                    {
                        // 만약 트랩 클래스 내부에 Release() 같은 해제 함수가 있다면 그것을 호출하는 것이 가장 좋고,
                        // 없다면 강제로 파괴(Destroy) 합니다.
                        Trap->Destroy();
                    }
                }

                // [물리 동기화] 앞으로 날리면서 살짝 띄움
                FVector LaunchDir = GetActorForwardVector();
                LaunchDir.Z = 0.5f;

                // 이제 이동 모드가 복구되었으므로 정상적으로 날아갑니다!
                Target->LaunchCharacter(LaunchDir * FollowUpKnockbackPower, true, true);

                // 데미지 적용
                UGameplayStatics::ApplyDamage(Target, 30.0f, GetController(), this, UDamageType::StaticClass());
            }
        }
    }
}

void AStage2Boss::Multicast_PlayBossMontage_Implementation(UAnimMontage* MontageToPlay)
{
    if (MontageToPlay)
    {
        PlayAnimMontage(MontageToPlay);
    }
}

void AStage2Boss::PerformMeleeHitCheck()
{
    if (!HasAuthority()) return;

    FVector StartLoc = GetActorLocation() + (GetActorForwardVector() * MeleeForwardOffset);
    StartLoc.Z += MeleeZOffset;

    DrawDebugSphere(GetWorld(), StartLoc, MeleeRadius, 16, FColor::Red, false, 2.0f);

    // [수정] FOverlapResult 배열 생성
    TArray<FOverlapResult> OverlapResults;
    FCollisionShape Sphere = FCollisionShape::MakeSphere(MeleeRadius);
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    bool bHit = GetWorld()->OverlapMultiByChannel(
        OverlapResults, StartLoc, FQuat::Identity, ECC_Pawn, Sphere, Params
    );

    if (bHit)
    {
        // [수정] FOverlapResult로 순회
        for (const FOverlapResult& Overlap : OverlapResults)
        {
            // [수정] Overlap.GetActor() 사용
            if (ABaseCharacter* HitPlayer = Cast<ABaseCharacter>(Overlap.GetActor()))
            {
                if (HitPlayer->IsDead()) continue;

                GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Orange, FString::Printf(TEXT("평타 적중!: %s"), *HitPlayer->GetName()));

                float DamageToApply = BossData ? BossData->AttackDamage : 20.0f;
                UGameplayStatics::ApplyDamage(HitPlayer, DamageToApply, GetController(), this, UDamageType::StaticClass());
            }
        }
    }
}