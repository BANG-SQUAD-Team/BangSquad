// Source/Project_Bang_Squad/Character/Enemy/Stage2MidBoss.cpp

#include "Project_Bang_Squad/Character/Enemy/Stage2MidBoss.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "NavigationSystem.h"
#include "Project_Bang_Squad/Character/MonsterBase/MidBossAIController.h" 
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"

AStage2MidBoss::AStage2MidBoss()
{
    // [Network]
    bReplicates = true;

    // [Collision]
    GetCapsuleComponent()->InitCapsuleSize(40.f, 90.0f);

    // [Component] 
    // HealthComponent가 부모에 없다면 생성, 있다면 부모 것 사용 (중복 생성 주의)
    if (!HealthComponent)
    {
        HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
    }

    CurrentPhase = EStage2Phase::Normal;

    // [Movement Rotation Setup]
    bUseControllerRotationYaw = false;

    if (GetCharacterMovement())
    {
        GetCharacterMovement()->bOrientRotationToMovement = true;
        GetCharacterMovement()->bUseControllerDesiredRotation = false; // AI가 제어하려면 false가 유리
        GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
        GetCharacterMovement()->MaxWalkSpeed = 600.0f; // 기본값
    }
}

void AStage2MidBoss::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AStage2MidBoss, CurrentPhase);
}

void AStage2MidBoss::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // [Data Driven]
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

        if (!HasAnyFlags(RF_ClassDefaultObject))
        {
            if (GetCharacterMovement())
            {
                GetCharacterMovement()->MaxWalkSpeed = BossData->WalkSpeed;
            }
            // 체력 등은 BeginPlay에서 설정
        }
    }
}

void AStage2MidBoss::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        // 체력 설정
        if (BossData && HealthComponent)
        {
            HealthComponent->SetMaxHealth(BossData->MaxHealth);
        }

        // AI에게 사거리 정보 전달
        if (BossData && GetController())
        {
            if (auto* MyAI = Cast<AMidBossAIController>(GetController()))
            {
                // AttackRange 변수는 BossData가 아니라 이 클래스의 UPROPERTY를 우선 사용하거나 동기화
                MyAI->SetAttackRange(AttackRange);
            }
        }
    }

    // 사망 이벤트 연결
    if (HealthComponent)
    {
        HealthComponent->OnDead.AddDynamic(this, &AStage2MidBoss::OnDeath);
    }
}

float AStage2MidBoss::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    if (ActualDamage <= 0.0f) return 0.0f;

    // AI에게 피격 알림
    if (GetController())
    {
        if (auto* MyAI = Cast<AMidBossAIController>(GetController()))
        {
            AActor* RealAttacker = DamageCauser;
            if (EventInstigator && EventInstigator->GetPawn())
            {
                RealAttacker = EventInstigator->GetPawn();
            }
            MyAI->OnDamaged(RealAttacker);
        }
    }
    return ActualDamage;
}

// --- [Combat Logic] ---

void AStage2MidBoss::PerformAttackTrace()
{
    if (!HasAuthority()) return;

    FVector Start = GetActorLocation();
    FVector Forward = GetActorForwardVector();
    FVector End = Start + (Forward * AttackRange);

    TArray<FHitResult> OutHits;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    bool bHit = GetWorld()->SweepMultiByChannel(
        OutHits, Start, End, FQuat::Identity, ECC_Pawn,
        FCollisionShape::MakeSphere(AttackRadius), Params
    );

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
    DrawDebugSphere(GetWorld(), End, AttackRadius, 12, bHit ? FColor::Green : FColor::Red, false, 1.0f);
#endif

    if (bHit)
    {
        TSet<AActor*> HitActors;
        for (const FHitResult& Hit : OutHits)
        {
            AActor* HitActor = Hit.GetActor();
            if (HitActor && !HitActors.Contains(HitActor))
            {
                UGameplayStatics::ApplyDamage(
                    HitActor,
                    BossData ? BossData->AttackDamage : 10.0f,
                    GetController(),
                    this,
                    UDamageType::StaticClass()
                );
                HitActors.Add(HitActor);
            }
        }
    }
}

void AStage2MidBoss::FireMagicMissile()
{
    // [최적화] AI Controller가 호출하므로 이미 서버입니다. 별도의 Server RPC 불필요.
    if (!HasAuthority()) return;

    // [수정] BossData->MagicProjectileClass 가 아니라 멤버 변수 MagicProjectileClass 사용
    if (!MagicProjectileClass)
    {
        // 디버그용: 투사체 미할당 시 빨간 선 표시
        DrawDebugLine(GetWorld(), GetActorLocation(), GetActorLocation() + GetActorForwardVector() * 500.f, FColor::Red, false, 2.0f);
        return;
    }

    FVector SpawnLoc = GetActorLocation();
    FRotator SpawnRot = GetActorRotation();

    // 소켓 우선 사용
    if (GetMesh()->DoesSocketExist(TEXT("Muzzle_01")))
        SpawnLoc = GetMesh()->GetSocketLocation(TEXT("Muzzle_01"));
    else if (GetMesh()->DoesSocketExist(TEXT("Hand_R")))
        SpawnLoc = GetMesh()->GetSocketLocation(TEXT("Hand_R"));

    // 타겟 방향 보정
    if (auto* MyAI = Cast<AMidBossAIController>(GetController()))
    {
        if (AActor* Target = MyAI->GetTargetActor())
        {
            FVector Direction = Target->GetActorLocation() - SpawnLoc;
            SpawnRot = Direction.Rotation();
        }
    }

    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.Instigator = this;

    // 투사체 생성
    GetWorld()->SpawnActor<AActor>(MagicProjectileClass, SpawnLoc, SpawnRot, Params);
}

void AStage2MidBoss::CastAreaSkill()
{
    if (HasAuthority())
    {
        UE_LOG(LogTemp, Log, TEXT("[Stage2Boss] Casting Area Skill!"));
        // TODO: 장판 스폰 로직
    }
}

bool AStage2MidBoss::TryTeleportToTarget(AActor* Target, float DistanceFromTarget)
{
    if (!HasAuthority() || !Target) return false;

    UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!NavSystem) return false;

    FVector TargetLoc = Target->GetActorLocation();
    FVector TargetBack = -Target->GetActorForwardVector();
    FVector DestLoc = TargetLoc + (TargetBack * DistanceFromTarget);

    FNavLocation NavLoc;
    // 200.0f 반경 내에서 갈 수 있는 땅 찾기
    if (NavSystem->ProjectPointToNavigation(DestLoc, NavLoc, FVector(200.0f)))
    {
        // [순서 중요]
        // 1. 사라지는 연출 (현재 위치)
        PlayTeleportAnim();

        // 2. 위치 이동 (높이 보정)
        float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        FVector FinalLoc = NavLoc.Location;
        FinalLoc.Z += CapsuleHalfHeight + 10.0f; // 땅에 묻히지 않게 살짝 띄움

        SetActorLocation(FinalLoc);

        // 3. 타겟 바라보기
        FVector LookDir = TargetLoc - FinalLoc;
        LookDir.Z = 0.f;
        SetActorRotation(LookDir.Rotation());

        // 4. 나타나는 이펙트 (Multicast)
        Multicast_TeleportEffect(FinalLoc);

        return true;
    }
    return false;
}

// --- [Animation Helpers] ---

float AStage2MidBoss::PlayMeleeAttackAnim()
{
    // [수정] BossData->... 가 아니라 멤버 변수 MeleeAttackMontage 사용
    if (!HasAuthority()) return 0.0f;
    if (MeleeAttackMontage)
    {
        Multicast_PlayMontage(MeleeAttackMontage);
        return MeleeAttackMontage->GetPlayLength();
    }
    return 0.0f;
}

float AStage2MidBoss::PlayMagicAttackAnim()
{
    // [수정] 멤버 변수 MagicAttackMontage 사용
    if (!HasAuthority()) return 0.0f;
    if (MagicAttackMontage)
    {
        Multicast_PlayMontage(MagicAttackMontage);
        return MagicAttackMontage->GetPlayLength();
    }
    return 0.0f;
}

float AStage2MidBoss::PlayTeleportAnim()
{
    // [수정] 멤버 변수 TeleportMontage 사용
    if (!HasAuthority()) return 0.0f;
    if (TeleportMontage)
    {
        Multicast_PlayMontage(TeleportMontage);
        return TeleportMontage->GetPlayLength();
    }
    return 0.0f;
}

void AStage2MidBoss::Multicast_PlayMontage_Implementation(UAnimMontage* MontageToPlay)
{
    if (MontageToPlay)
    {
        PlayAnimMontage(MontageToPlay);
    }
}

void AStage2MidBoss::Multicast_TeleportEffect_Implementation(FVector Location)
{
    if (TeleportEffect)
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), TeleportEffect, Location);
    }
}

// --- [State Logic] ---

void AStage2MidBoss::SetPhase(EStage2Phase NewPhase)
{
    if (HasAuthority())
    {
        CurrentPhase = NewPhase;
        OnRep_CurrentPhase();
    }
}

void AStage2MidBoss::OnRep_CurrentPhase()
{
    // 페이즈 변화에 따른 비주얼 처리
}

void AStage2MidBoss::OnDeath()
{
    SetPhase(EStage2Phase::Dead);

    if (auto* MyAI = Cast<AMidBossAIController>(GetController()))
    {
        MyAI->StopMovement();
        MyAI->UnPossess(); // 혹은 AI 정지 함수 호출
    }

    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 사망 몽타주 재생 (DataAsset에 있다면)
    if (BossData && BossData->DeathMontage)
    {
        PlayAnimMontage(BossData->DeathMontage);
    }

    if (HasAuthority())
    {
        SetLifeSpan(5.0f);
    }
}