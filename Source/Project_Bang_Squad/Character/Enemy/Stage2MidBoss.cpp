// Stage2MidBoss.cpp

#include "Stage2MidBoss.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h" // SphereTrace 디버그용
#include "Project_Bang_Squad/Character/MonsterBase/MidBossAIController.h" 

AStage2MidBoss::AStage2MidBoss()
{
    // [Network] 멀티플레이어 동기화 활성화
    bReplicates = true;

    // [Collision] 마법사는 기사보다 피격 판정을 약간 얇게 설정
    GetCapsuleComponent()->InitCapsuleSize(40.f, 90.0f);

    // [Component] 체력 관리 컴포넌트 부착
    HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));

    // *중요* Stage 1과 달리 WeaponCollisionBox(칼 판정)를 생성하지 않음!
    // -> 메모리 절약 및 로직 분리

    CurrentPhase = EStage2Phase::Normal;
}

void AStage2MidBoss::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // [Replication] 페이즈 상태는 게임플레이에 중요하므로 서버->클라 복제
    DOREPLIFETIME(AStage2MidBoss, CurrentPhase);
}

void AStage2MidBoss::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // [Data Driven Design]
    // 블루프린트 컴파일 시점(OnConstruction)에 DataAsset 내용을 읽어와서
    // 캐릭터의 외형, 애니메이션, 스탯을 자동으로 세팅합니다.
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

        // CDO(Class Default Object) 체크: 런타임이 아닌 에디터 기본값 설정 방지
        if (!HasAnyFlags(RF_ClassDefaultObject))
        {
            if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
            {
                MoveComp->MaxWalkSpeed = BossData->WalkSpeed;
            }
        }
    }
}

void AStage2MidBoss::BeginPlay()
{
    Super::BeginPlay();

    // [Server Authority] 체력 초기화 등 핵심 로직은 서버 권한 확인 후 실행
    if (HasAuthority())
    {
        if (BossData && HealthComponent)
        {
            HealthComponent->SetMaxHealth(BossData->MaxHealth);
            UE_LOG(LogTemp, Log, TEXT("[Stage2Boss] Initialized Health: %f"), BossData->MaxHealth);
        }
    }

    // 사망 델리게이트 연결 (서버/클라 모두 연결하여 각자 처리할 로직 수행)
    if (HealthComponent)
    {
        HealthComponent->OnDead.AddDynamic(this, &AStage2MidBoss::OnDeath);
    }
}

float AStage2MidBoss::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    if (ActualDamage <= 0.0f) return 0.0f;

    // [AI Interaction] 피격 시 AI 컨트롤러에게 "나 맞았어, 공격자 확인해" 신호 전달
    if (GetController())
    {
        auto* MyAI = Cast<AMidBossAIController>(GetController());
        if (MyAI)
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

// 1. Sphere Trace (지팡이 휘두르기 / 근접 충격파)
void AStage2MidBoss::PerformAttackTrace()
{
    // [Authority] 데미지 판정은 무조건 서버에서만! (치팅 방지)
    if (!HasAuthority()) return;

    FVector Start = GetActorLocation();
    FVector Forward = GetActorForwardVector();
    FVector End = Start + (Forward * AttackRange);

    TArray<FHitResult> OutHits;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this); // 자해 방지

    // 플레이어(Pawn)만 감지하도록 설정
    bool bHit = GetWorld()->SweepMultiByChannel(
        OutHits,
        Start,
        End,
        FQuat::Identity,
        ECC_Pawn,
        FCollisionShape::MakeSphere(AttackRadius),
        Params
    );

    // [Debug] 개발 빌드에서만 범위 표시 (빨간 구체)
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
    DrawDebugSphere(GetWorld(), End, AttackRadius, 12, bHit ? FColor::Green : FColor::Red, false, 1.0f);
#endif

    if (bHit)
    {
        TSet<AActor*> HitActors; // 중복 타격 방지
        for (const FHitResult& Hit : OutHits)
        {
            AActor* HitActor = Hit.GetActor();
            if (HitActor && !HitActors.Contains(HitActor))
            {
                // 데미지 적용
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

// 2. Projectile (유도탄 발사)
void AStage2MidBoss::FireMagicMissile()
{
    // [Authority] 클라이언트가 호출해도 서버에서 실행되도록 체크
    if (HasAuthority())
    {
        FVector SpawnLoc = GetActorLocation() + GetActorForwardVector() * 100.f;
        FRotator SpawnRot = GetActorRotation();

        // 소켓 우선 사용 ("Muzzle_01" -> "Hand_R" 순서)
        if (GetMesh()->DoesSocketExist(TEXT("Muzzle_01")))
        {
            SpawnLoc = GetMesh()->GetSocketLocation(TEXT("Muzzle_01"));
        }
        else if (GetMesh()->DoesSocketExist(TEXT("Hand_R")))
        {
            SpawnLoc = GetMesh()->GetSocketLocation(TEXT("Hand_R"));
        }

        // 실제 생성은 Server RPC로 위임
        Server_SpawnMagicProjectile(SpawnLoc, SpawnRot);
    }
}

void AStage2MidBoss::Server_SpawnMagicProjectile_Implementation(FVector SpawnLoc, FRotator SpawnRot)
{
    // [Safety] DataAsset에 투사체 클래스가 지정되어 있는지 확인
    if (!BossData || !BossData->MagicProjectileClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Stage2Boss] MagicProjectileClass is NULL in DataAsset!"));
        return;
    }

    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.Instigator = this; // 투사체가 생성자(보스)를 무시하도록 설정

    GetWorld()->SpawnActor<AActor>(BossData->MagicProjectileClass, SpawnLoc, SpawnRot, Params);
}

void AStage2MidBoss::CastAreaSkill()
{
    if (HasAuthority())
    {
        // TODO: 광역기(장판) 소환 로직 구현
        UE_LOG(LogTemp, Log, TEXT("[Stage2Boss] Casting Area Skill (Meteor)!"));
    }
}

// --- [Animation Helpers] ---

float AStage2MidBoss::PlayMagicAttackAnim()
{
    if (!HasAuthority()) return 0.0f;

    if (BossData && BossData->MagicAttackMontage)
    {
        Multicast_PlayMontage(BossData->MagicAttackMontage);
        return BossData->MagicAttackMontage->GetPlayLength();
    }
    return 0.0f;
}

float AStage2MidBoss::PlayTeleportAnim()
{
    if (!HasAuthority()) return 0.0f;

    if (BossData && BossData->TeleportMontage)
    {
        Multicast_PlayMontage(BossData->TeleportMontage);
        return BossData->TeleportMontage->GetPlayLength();
    }
    return 0.0f;
}

void AStage2MidBoss::Multicast_PlayMontage_Implementation(UAnimMontage* MontageToPlay)
{
    // [Visual Sync] 서버뿐만 아니라 모든 클라이언트에서 애니메이션 재생
    if (MontageToPlay)
    {
        PlayAnimMontage(MontageToPlay);
    }
}

// --- [State & Phase] ---

void AStage2MidBoss::SetPhase(EStage2Phase NewPhase)
{
    if (HasAuthority())
    {
        CurrentPhase = NewPhase;
        OnRep_CurrentPhase(); // 서버에서는 OnRep이 자동 호출되지 않으므로 수동 호출
    }
}

void AStage2MidBoss::OnRep_CurrentPhase()
{
    // [Visual] 페이즈 변경에 따른 시각적 변화 처리 (보호막, 머티리얼 변경 등)
    switch (CurrentPhase)
    {
    case EStage2Phase::Gimmick:
        // 예: 무적 보호막 이펙트 켜기
        break;
    case EStage2Phase::Dead:
        // 충돌 제거 및 사망 애니메이션
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        if (BossData && BossData->DeathMontage)
        {
            PlayAnimMontage(BossData->DeathMontage);
        }
        break;
    }
}

void AStage2MidBoss::OnDeath()
{
    SetPhase(EStage2Phase::Dead);

    // AI 컨트롤러 정지
    if (auto* MyAI = Cast<AMidBossAIController>(GetController()))
    {
        // MyAI->StopBehaviorTree(); // 혹은 SetDeadState()
        MyAI->SetDeadState();
    }

    if (HasAuthority())
    {
        SetLifeSpan(5.0f); // 5초 뒤 시체 삭제
    }
}