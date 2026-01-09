#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h"

#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

//  여기서 HealthComponent 헤더 경로 확인 필요
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"

AEnemyCharacterBase::AEnemyCharacterBase()
{
    PrimaryActorTick.bCanEverTick = false;
    bUseControllerRotationYaw = false;

    bReplicates = true;
}

void AEnemyCharacterBase::BeginPlay()
{
    Super::BeginPlay();

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
       DefaultMaxWalkSpeed = Move->MaxWalkSpeed;

       Move->bUseControllerDesiredRotation = true;
       Move->RotationRate = FRotator(0.f, 720.f, 0.f);
    }

    // =========================
    // HealthComponent 자동 바인딩
    // =========================
    if (UHealthComponent* HC = FindComponentByClass<UHealthComponent>())
    {
       HC->OnDead.RemoveDynamic(this, &AEnemyCharacterBase::HandleDeadFromHealth);
       HC->OnDead.AddDynamic(this, &AEnemyCharacterBase::HandleDeadFromHealth);

       HC->OnHealthChanged.RemoveDynamic(this, &AEnemyCharacterBase::HandleHealthChangedFromHealth);
       HC->OnHealthChanged.AddDynamic(this, &AEnemyCharacterBase::HandleHealthChangedFromHealth);
    }
}

// [추가됨] 데미지를 받아서 HealthComponent로 넘겨주는 핵심 함수
float AEnemyCharacterBase::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
    // 1. 부모 클래스의 기본 로직 실행
    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    // 2. 서버 권한 확인 (데미지 처리는 서버에서만)
    if (HasAuthority())
    {
        // 3. HealthComponent를 찾아서 데미지 적용
        if (UHealthComponent* HC = FindComponentByClass<UHealthComponent>())
        {
            // 여기서 HealthComponent의 체력이 깎이고 -> OnHealthChanged 델리게이트가 호출됨
            HC->ApplyDamage(ActualDamage);
        }
    }

    return ActualDamage;
}

void AEnemyCharacterBase::OnDeathStarted()
{
}

void AEnemyCharacterBase::HandleDeadFromHealth()
{
    ReceiveDeath();
}

void AEnemyCharacterBase::HandleHealthChangedFromHealth(float NewHealth, float InMaxHealth)
{
    if (!HasAuthority()) return;
    if (bIsDead) return;
    if (NewHealth <= 0.f) return;

    // 데미지를 입었으니 피격 모션 재생
    ReceiveHitReact();
}

// =========================
// Hit React
// =========================

void AEnemyCharacterBase::ReceiveHitReact()
{
    if (!HasAuthority()) return;
    if (bIsDead) return;

    if (bIsHitReacting && bIgnoreHitReactWhileActive)
    {
       return;
    }

    float Duration = HitReactMinDuration;
    if (HitReactMontage)
    {
       Duration = FMath::Max(Duration, HitReactMontage->GetPlayLength());
    }

    StartHitReact(Duration);
}

void AEnemyCharacterBase::StartHitReact(float Duration)
{
    bIsHitReacting = true;

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
       if (DefaultMaxWalkSpeed <= 0.f)
       {
          DefaultMaxWalkSpeed = Move->MaxWalkSpeed;
       }

       Move->MaxWalkSpeed = DefaultMaxWalkSpeed * HitReactSpeedMultiplier;
    }

    Multicast_PlayHitReactMontage();

    GetWorldTimerManager().ClearTimer(HitReactTimer);
    GetWorldTimerManager().SetTimer(HitReactTimer, this, &AEnemyCharacterBase::EndHitReact, Duration, false);
}

void AEnemyCharacterBase::EndHitReact()
{
    bIsHitReacting = false;

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
       Move->MaxWalkSpeed = DefaultMaxWalkSpeed;
    }
}

void AEnemyCharacterBase::Multicast_PlayHitReactMontage_Implementation()
{
    if (!HitReactMontage) return;
    if (bIsDead) return;

    PlayAnimMontage(HitReactMontage);
}

// =========================
// Death
// =========================

void AEnemyCharacterBase::ReceiveDeath()
{
    if (!HasAuthority()) return;
    if (bIsDead) return;

    StartDeath();
}

void AEnemyCharacterBase::StartDeath()
{
    bIsDead = true;
    bIsHitReacting = false;

    OnDeathStarted();

    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
       AIC->StopMovement();
       AIC->ClearFocus(EAIFocusPriority::Gameplay);
    }

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
       Move->StopMovementImmediately();
       Move->DisableMovement();
    }

    Multicast_PlayDeathMontage();

    GetWorldTimerManager().ClearTimer(DeathToRagdollTimer);
    if (bEnableRagdollOnDeath)
    {
       GetWorldTimerManager().SetTimer(
          DeathToRagdollTimer,
          this,
          &AEnemyCharacterBase::EnterRagdoll,
          DeathToRagdollDelay,
          false
       );
    }

    if (bDestroyAfterDeath)
    {
       SetLifeSpan(DestroyDelay);
    }
}

void AEnemyCharacterBase::Multicast_PlayDeathMontage_Implementation()
{
    if (!DeathMontage) return;

    PlayAnimMontage(DeathMontage);

    if (bLoopDeathMontage && DeathLoopSectionName != NAME_None)
    {
       if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
       {
          AnimInst->Montage_SetNextSection(DeathLoopSectionName, DeathLoopSectionName, DeathMontage);
       }
    }
}

void AEnemyCharacterBase::EnterRagdoll()
{
    if (!HasAuthority()) return;
    Multicast_EnterRagdoll();
}

void AEnemyCharacterBase::Multicast_EnterRagdoll_Implementation()
{
    if (!GetMesh()) return;

    if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
    {
       AnimInst->StopAllMontages(0.1f);
    }

    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
       Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
    GetMesh()->SetSimulatePhysics(true);
    GetMesh()->WakeAllRigidBodies();
    GetMesh()->bBlendPhysics = true;
}