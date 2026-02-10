// Source/Project_Bang_Squad/Character/StageBoss/Stage2Boss.cpp

#include "Project_Bang_Squad/Character/StageBoss/Stage2Boss.h"
#include "Project_Bang_Squad/Character/StageBoss/Stage2SpiderAIController.h"
#include "Project_Bang_Squad/Character/Enemy/EnemySpawner.h"
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

AStage2Boss::AStage2Boss()
{
    AIControllerClass = AStage2SpiderAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
}

void AStage2Boss::BeginPlay()
{
    Super::BeginPlay();

    if (BossData && HealthComponent)
    {
        HealthComponent->SetMaxHealth(BossData->MaxHealth);
    }
}

void AStage2Boss::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

float AStage2Boss::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (bIsInvincible) return 0.0f;

    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    // 체력 페이즈 체크
    CheckHealthPhase();

    return ActualDamage;
}

void AStage2Boss::CheckHealthPhase()
{
    if (!HealthComponent) return;

    // [수정] HealthComponent 내부 수정 없이 직접 비율 계산
    float MaxHP = HealthComponent->GetMaxHealth();
    float CurHP = HealthComponent->GetHealth();
    float HPRatio = (MaxHP > 0.0f) ? (CurHP / MaxHP) : 0.0f;

    // 70% 페이즈
    if (!bPhase70Triggered && HPRatio <= 0.7f)
    {
        bPhase70Triggered = true;
        bIsInvincible = true;

        if (auto* AI = Cast<AStage2SpiderAIController>(GetController()))
            AI->StartPhasePattern();

        // [수정] StartSpawning -> SetSpawnerActive
        if (MinionSpawner)
            MinionSpawner->SetSpawnerActive(true);

        FTimerHandle CheckHandle;
        GetWorld()->GetTimerManager().SetTimer(CheckHandle, this, &AStage2Boss::CheckMinionsStatus, 1.0f, true);

        UE_LOG(LogTemp, Warning, TEXT("Boss Phase 1 (70 Percent) Started!"));
    }
    // 30% 페이즈
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

void AStage2Boss::CheckMinionsStatus()
{
    // [필요] EnemySpawner에 GetCurrentEnemyCount() 함수가 필요합니다.
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

void AStage2Boss::PerformWebShot(AActor* Target)
{
    if (!Target || !WebProjectileClass) return;

    FVector Direction = Target->GetActorLocation() - GetActorLocation();
    SetActorRotation(Direction.Rotation());

    FVector SpawnLoc = GetActorLocation() + GetActorForwardVector() * 100.0f + FVector(0, 0, 50);
    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.Instigator = this;

    AActor* Projectile = GetWorld()->SpawnActor<AActor>(WebProjectileClass, SpawnLoc, Direction.Rotation(), Params);

    if (Projectile)
    {
        UProjectileMovementComponent* PMC = Projectile->FindComponentByClass<UProjectileMovementComponent>();
        if (PMC)
        {
            PMC->InitialSpeed = WebProjectileSpeed;
            PMC->MaxSpeed = WebProjectileSpeed;
        }
    }

    if (WebShotMontage) PlayAnimMontage(WebShotMontage);
}

void AStage2Boss::PerformSmashAttack(AActor* Target)
{
    if (SmashMontage) PlayAnimMontage(SmashMontage);
}

void AStage2Boss::StartQTEPattern(AActor* Target)
{
    // QTE 시작 로직
}

void AStage2Boss::OnQTEResult(bool bSuccess)
{
    if (bSuccess)
    {
        UE_LOG(LogTemp, Log, TEXT("QTE Success!"));
        // 그로기 상태 등 처리
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("QTE Failed!"));
        // 전멸기 등 처리
    }
}

float AStage2Boss::PlayMeleeAttackAnim()
{
    if (BossData && BossData->AttackMontages.Num() > 0)
    {
        return PlayAnimMontage(BossData->AttackMontages[0]);
    }
    return 0.0f;
}