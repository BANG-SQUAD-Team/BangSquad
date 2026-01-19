#include "JobCrystal.h"
#include "StageBossBase.h"

// [중요] 캐스팅을 위해 모든 캐릭터 헤더 포함
#include "Project_Bang_Squad/Character/TitanCharacter.h"
#include "Project_Bang_Squad/Character/StrikerCharacter.h"
#include "Project_Bang_Squad/Character/MageCharacter.h"
#include "Project_Bang_Squad/Character/PaladinCharacter.h"

AJobCrystal::AJobCrystal()
{
    bReplicates = true;
    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    RootComponent = MeshComp;
    MeshComp->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    CurrentHealth = MaxHealth;
}

float AJobCrystal::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (!HasAuthority()) return 0.0f;

    // 공격 주체 확인 (투사체일 경우 Owner 확인)
    AActor* Attacker = DamageCauser;
    if (Attacker && !Attacker->IsA(ACharacter::StaticClass()))
    {
        Attacker = Attacker->GetOwner();
    }
    if (!Attacker) return 0.0f;

    // --- [직업 체크 로직] ---
    bool bIsCorrectJob = false;

    switch (RequiredJobType)
    {
    case EJobType::Titan:
        bIsCorrectJob = (Cast<ATitanCharacter>(Attacker) != nullptr);
        break;
    case EJobType::Striker:
        bIsCorrectJob = (Cast<AStrikerCharacter>(Attacker) != nullptr);
        break;
    case EJobType::Mage:
        bIsCorrectJob = (Cast<AMageCharacter>(Attacker) != nullptr);
        break;
    case EJobType::Paladin:
        bIsCorrectJob = (Cast<APaladinCharacter>(Attacker) != nullptr);
        break;
    default:
        break;
    }

    if (!bIsCorrectJob)
    {
        // 직업 불일치 -> 데미지 무효
        return 0.0f;
    }

    CurrentHealth -= DamageAmount;

    if (CurrentHealth <= 0.0f)
    {
        if (TargetBoss)
        {
            TargetBoss->OnGimmickResolved(0);
        }
        Destroy();
    }

    return DamageAmount;
}