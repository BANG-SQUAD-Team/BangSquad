// Source/Project_Bang_Squad/Character/StageBoss/JobCrystal.cpp
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

    // [설정] 2대 맞으면 깨지도록 초기화 (생성자에서도 확실하게)
    MaxHealth = 2.0f;
    CurrentHealth = MaxHealth;
}

float AJobCrystal::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    // 1. [서버 권한] 확인
    if (!HasAuthority()) return 0.0f;

    // 2. [공격자 확인] (투사체일 경우 Owner 확인)
    AActor* Attacker = DamageCauser;
    if (Attacker && !Attacker->IsA(ACharacter::StaticClass()))
    {
        Attacker = Attacker->GetOwner();
    }
    if (!Attacker) return 0.0f;

    // 3. [직업 체크 로직]
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

    // ==========================================================================================
    // [테스트/배포 분기점]
    // 나중에 4인 멀티 테스트 시에는 아래 if문 안의 '주석'을 해제하고, 로그 부분을 지우세요.
    // ==========================================================================================
    if (!bIsCorrectJob)
    {
        // [TODO: 정식 버전에서는 아래 주석(return 0.0f)을 해제하여 타 직업 데미지 무효화 필수!]
        // return 0.0f; 

        // [TEST MODE] 지금은 테스트 중이니 경고만 띄우고 데미지 들어감
        UE_LOG(LogTemp, Warning, TEXT("[TEST] Job Mismatch! But damage allowed. (Required: %d)"), (int32)RequiredJobType);
    }

    // 4. [데미지 적용] "무조건 1씩 차감" (평타 2방 컷 로직)
    // 들어온 DamageAmount는 무시하고 1만 깎습니다.
    CurrentHealth -= 1.0f;

    // 5. [파괴 처리]
    if (CurrentHealth <= 0.0f)
    {
        if (TargetBoss)
        {
            TargetBoss->OnGimmickResolved(0);
        }

        // 파괴!
        Destroy();
    }

    return DamageAmount;
}