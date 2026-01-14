#include "EnemyNormal.h"

#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h" // [필수]
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h" // [필수] 플레이어 헤더

AEnemyNormal::AEnemyNormal()
{
    PrimaryActorTick.bCanEverTick = false;
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

void AEnemyNormal::AcquireTarget()
{
    APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
    TargetPawn = PlayerPawn;
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
    }

    TargetPawn = nullptr;
}

void AEnemyNormal::UpdateMoveTo()
{
    if (IsDead()) return;

    APawn* TP = TargetPawn.Get();
    if (!TP)
    {
       StopChase();
       return;
    }

    const float Dist = FVector::Dist(GetActorLocation(), TP->GetActorLocation());
    if (Dist > StopChaseDistance)
    {
       StopChase();
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

    // 1. 유효한 공격 설정 찾기 (Struct 배열 순회)
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

    // 2. 랜덤 선택
    const int32 Pick = ValidIndices[FMath::RandRange(0, ValidIndices.Num() - 1)];
    const FEnemyAttackData& SelectedAttack = AttackConfigs[Pick];

    LastAttackTime = Now;
    bIsAttacking = true;

    // 3. 공격 애니메이션 재생 (Index 전송)
    Multicast_PlayAttackMontage(Pick);

    // ====================================================
    // [구조체에 설정된 시간 값을 사용하여 타이머 설정
    // ====================================================

    // 4. 선딜 후 콜리전 켜기
    GetWorldTimerManager().SetTimer(
        CollisionEnableTimer,
        this,
        &AEnemyNormal::EnableWeaponCollision,
        SelectedAttack.HitDelay, // 각 공격별 선딜 시간 적용
        false
    );

    // 5. 지속 시간 후 콜리전 끄기
    // 끄는 시점 = (선딜 + 지속시간)
    float DisableTime = SelectedAttack.HitDelay + SelectedAttack.HitDuration;
    
    GetWorldTimerManager().SetTimer(
        CollisionDisableTimer,
        this,
        &AEnemyNormal::DisableWeaponCollision,
        DisableTime,
        false
    );

    // 6. 전체 동작 종료 타이머
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
    // 인덱스 범위 체크
    if (!AttackConfigs.IsValidIndex(MontageIndex)) return;

    UAnimMontage* MontageToPlay = AttackConfigs[MontageIndex].Montage;
    if (!MontageToPlay) return;

    PlayAnimMontage(MontageToPlay);
}

// [추가] 콜리전 켜기
void AEnemyNormal::EnableWeaponCollision()
{
    if (WeaponCollisionBox)
    {
        WeaponCollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    }
}

// [추가] 콜리전 끄기
void AEnemyNormal::DisableWeaponCollision()
{
    if (WeaponCollisionBox)
    {
        WeaponCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

// [추가] 실제 충돌 감지 (여기서 데미지 줌)
void AEnemyNormal::OnWeaponOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
                                   UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
                                   bool bFromSweep, const FHitResult& SweepResult)
{
    if (!HasAuthority()) return;

    if (OtherActor == nullptr || OtherActor == this || OtherActor == GetOwner()) return;

    if (OtherActor->IsA(ABaseCharacter::StaticClass()))
    {
        UGameplayStatics::ApplyDamage(
            OtherActor,
            AttackDamage,
            GetController(),
            this,
            UDamageType::StaticClass()
        );

        // 한 번 때렸으면 즉시 판정 꺼서 다단히트 방지
        DisableWeaponCollision();
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

    // 모든 타이머 및 공격 판정 초기화
    GetWorldTimerManager().ClearTimer(RepathTimer);
    GetWorldTimerManager().ClearTimer(AttackEndTimer);
    GetWorldTimerManager().ClearTimer(CollisionEnableTimer);
    GetWorldTimerManager().ClearTimer(CollisionDisableTimer);
    
    StopChase();
    bIsAttacking = false;
    DisableWeaponCollision(); 
}