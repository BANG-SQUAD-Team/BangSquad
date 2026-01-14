// EnemyMidBoss.cpp

#include "EnemyMidBoss.h"
#include "Project_Bang_Squad/Character/MonsterBase/MidBossAIController.h" // [중요] AI와 통신하기 위해 헤더 포함
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

AEnemyMidBoss::AEnemyMidBoss()
{
    // 1. 기본 설정
    GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
    CurrentPhase = EMidBossPhase::Normal;
    bReplicates = true;

    // 2. 컴포넌트 생성
    HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
}

void AEnemyMidBoss::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AEnemyMidBoss, CurrentPhase);
}

void AEnemyMidBoss::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // Data Asset을 이용하여 캐릭터 설정 덮어씌우기
    if (BossData)
    {
        if (GetMesh() && BossData->Mesh)
        {
            GetMesh()->SetSkeletalMesh(BossData->Mesh);
        }
        if (GetMesh() && BossData->AnimClass)
        {
            GetMesh()->SetAnimInstanceClass(BossData->AnimClass);
        }
        if (GetCharacterMovement())
        {
            GetCharacterMovement()->MaxWalkSpeed = BossData->WalkSpeed;
        }
    }
}

void AEnemyMidBoss::BeginPlay()
{
    Super::BeginPlay();

    // 서버 권한 확인 후 체력 초기화
    if (HasAuthority() && BossData && HealthComponent)
    {
        HealthComponent->SetMaxHealth(BossData->MaxHealth);
        UE_LOG(LogTemp, Log, TEXT("[MidBoss] Initialized Health: %f"), BossData->MaxHealth);
    }
}

// [핵심 로직] 데미지 처리 및 AI 전달
float AEnemyMidBoss::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    // 유효하지 않은 데미지거나 이미 죽었다면 무시
    if (ActualDamage <= 0.0f) return 0.0f;
    if (HealthComponent && HealthComponent->IsDead()) return 0.0f;

    // 1. HealthComponent에 데미지 적용
    if (HealthComponent)
    {
        HealthComponent->ApplyDamage(ActualDamage);
    }

    // 2. AI Controller에게 피격 사실 알림 (반격 유도)
    if (GetController())
    {
        auto* MyAI = Cast<AMidBossAIController>(GetController());
        if (MyAI)
        {
            // 때린 대상(DamageCauser) 정보를 넘겨줌
            MyAI->OnDamaged(DamageCauser);
        }
    }

    return ActualDamage;
}

// --- [Animation Helper Functions] ---

float AEnemyMidBoss::PlayAggroAnim()
{
    if (BossData && BossData->AggroMontage)
    {
        return PlayAnimMontage(BossData->AggroMontage);
    }
    return 0.0f;
}

float AEnemyMidBoss::PlayRandomAttack()
{
    if (BossData && BossData->AttackMontages.Num() > 0)
    {
        int32 RandomIndex = FMath::RandRange(0, BossData->AttackMontages.Num() - 1);
        UAnimMontage* SelectedMontage = BossData->AttackMontages[RandomIndex];
        if (SelectedMontage)
        {
            return PlayAnimMontage(SelectedMontage);
        }
    }
    return 0.0f;
}

float AEnemyMidBoss::PlayHitReactAnim()
{
    if (BossData && BossData->HitReactMontage)
    {
        return PlayAnimMontage(BossData->HitReactMontage);
    }
    return 0.0f;
}

// --- [Phase Control] ---

void AEnemyMidBoss::SetPhase(EMidBossPhase NewPhase)
{
    if (HasAuthority())
    {
        CurrentPhase = NewPhase;
        OnRep_CurrentPhase(); // 서버에서도 연출 재생
    }
}

void AEnemyMidBoss::OnRep_CurrentPhase()
{
    switch (CurrentPhase)
    {
    case EMidBossPhase::Normal:
        break;
    case EMidBossPhase::Gimmick:
        UE_LOG(LogTemp, Warning, TEXT("[MidBoss] Phase GIMMICK Start! (FX)"));
        break;
    case EMidBossPhase::Dead:
        // 사망 처리 (필요시 캡슐 콜리전 해제 등 추가)
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        break;
    }
}