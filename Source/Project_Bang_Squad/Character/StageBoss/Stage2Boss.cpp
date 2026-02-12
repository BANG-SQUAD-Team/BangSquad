// Source/Project_Bang_Squad/Character/StageBoss/Stage2Boss.cpp

#include "Project_Bang_Squad/Character/StageBoss/Stage2Boss.h"
#include "Project_Bang_Squad/Character/StageBoss/Stage2SpiderAIController.h"
#include "Project_Bang_Squad/Character/Enemy/EnemySpawner.h"
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h" // 헤더 추가

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

    // [����] HealthComponent ���� ���� ���� ���� ���� ���
    float MaxHP = HealthComponent->GetMaxHealth();
    float CurHP = HealthComponent->GetHealth();
    float HPRatio = (MaxHP > 0.0f) ? (CurHP / MaxHP) : 0.0f;

    // 70% ������
    if (!bPhase70Triggered && HPRatio <= 0.7f)
    {
        bPhase70Triggered = true;
        bIsInvincible = true;

        if (auto* AI = Cast<AStage2SpiderAIController>(GetController()))
            AI->StartPhasePattern();

        // [����] StartSpawning -> SetSpawnerActive
        if (MinionSpawner)
            MinionSpawner->SetSpawnerActive(true);

        FTimerHandle CheckHandle;
        GetWorld()->GetTimerManager().SetTimer(CheckHandle, this, &AStage2Boss::CheckMinionsStatus, 1.0f, true);

        UE_LOG(LogTemp, Warning, TEXT("Boss Phase 1 (70 Percent) Started!"));
    }
    // 30% ������
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
        PlayAnimMontage(WebShotMontage);
    }
    else
    {
        // 만약 몽타주가 없으면? 비상용으로 즉시 발사 (테스트용)
        FireWebProjectile();
    }
}

void AStage2Boss::FireWebProjectile()
{
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
    if (SmashMontage) PlayAnimMontage(SmashMontage);
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
        return PlayAnimMontage(BossData->AttackMontages[0]);
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