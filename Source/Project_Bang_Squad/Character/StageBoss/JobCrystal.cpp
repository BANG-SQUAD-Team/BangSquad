// Source/Project_Bang_Squad/Character/StageBoss/JobCrystal.cpp
#include "JobCrystal.h"
#include "StageBossBase.h"
#include "Kismet/GameplayStatics.h"    // [�ʼ�] GetActorOfClass ���
#include "Stage1Boss.h"                // [�ʼ�] ���� Ŭ���� �ν��� ���� ��� ����
// [�߿�] ĳ������ ���� ��� ĳ���� ��� ����
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

    // [����] 2�� ������ �������� �ʱ�ȭ (�����ڿ����� Ȯ���ϰ�)
    MaxHealth = 2.0f;
    CurrentHealth = MaxHealth;
}

float AJobCrystal::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    // 1. [���� ����] Ȯ��
    if (!HasAuthority()) return 0.0f;

    // 2. [������ Ȯ��] (����ü�� ��� Owner Ȯ��)
    AActor* Attacker = DamageCauser;
    if (Attacker && !Attacker->IsA(ACharacter::StaticClass()))
    {
        Attacker = Attacker->GetOwner();
    }
    if (!Attacker) return 0.0f;

    // 3. [���� üũ ����]
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
    // [�׽�Ʈ/���� �б���]
    // ���߿� 4�� ��Ƽ �׽�Ʈ �ÿ��� �Ʒ� if�� ���� '�ּ�'�� �����ϰ�, �α� �κ��� ���켼��.
    // ==========================================================================================
    if (!bIsCorrectJob)
    {
        // [TODO: ���� ���������� �Ʒ� �ּ�(return 0.0f)�� �����Ͽ� Ÿ ���� ������ ��ȿȭ �ʼ�!]
         return 0.0f; 

        // [TEST MODE] ������ �׽�Ʈ ���̴� ��� ���� ������ ��
       // UE_LOG(LogTemp, Warning, TEXT("[TEST] Job Mismatch! But damage allowed. (Required: %d)"), (int32)RequiredJobType);
    }

    // 4. [������ ����] "������ 1�� ����" (��Ÿ 2�� �� ����)
    // ���� DamageAmount�� �����ϰ� 1�� ����ϴ�.
    CurrentHealth -= 1.0f;

    // 5. [�ı� ó��]
    if (CurrentHealth <= 0.0f)
    {
        if (TargetBoss)
        {
            TargetBoss->OnGimmickResolved(0);
        }

        // �ı�!
        Destroy();
    }

    return DamageAmount;
}

void AJobCrystal::BeginPlay()
{
    Super::BeginPlay();

    // ���� ������ ���� �� �Ǿ� �ִٸ�? (�ʿ� ���� ��ġ�� ���)
    if (TargetBoss == nullptr && HasAuthority())
    {
        // ���忡 �ִ� 'AStage1Boss'�� ã�Ƽ� �� ������ �Ӹ��Ѵ�.
        AActor* FoundBoss = UGameplayStatics::GetActorOfClass(GetWorld(), AStage1Boss::StaticClass());
        if (FoundBoss)
        {
            TargetBoss = Cast<AStage1Boss>(FoundBoss);
            UE_LOG(LogTemp, Warning, TEXT("JobCrystal: Auto-connected to Boss manually!"));
        }
    }
}
