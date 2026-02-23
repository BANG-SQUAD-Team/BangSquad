#include "Project_Bang_Squad/Character/PaladinCharacter.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Project_Bang_Squad/Character/Player/Paladin/PaladinSkill2Hammer.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/DataTable.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Engine/DamageEvents.h"
#include "DrawDebugHelpers.h" 
#include "NiagaraFunctionLibrary.h"

// ====================================================================================
//  [Section 1] 생성자 및 초기화 (Constructor & Initialization)
// ====================================================================================

APaladinCharacter::APaladinCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);

    // 1. 기본 스탯 및 물리 설정
    AttackCooldownTime = 1.f;
    
    // 컨트롤러 회전 설정 (캐릭터는 이동 방향을 봄)
    bUseControllerRotationYaw = true;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;

    if (UCharacterMovementComponent* CharMove = GetCharacterMovement())
    {
        CharMove->MaxWalkSpeed = 550.f;
        CharMove->bOrientRotationToMovement = false; // 컨트롤러 방향을 따르므로 false
        CharMove->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
    }

    // 2. 카메라 설정
    if (SpringArm)
    {
        SpringArm->TargetOffset = FVector(0.0f, 0.0f, 150.0f);
        SpringArm->ProbeSize = 12.0f;
    }

    // 3. 방패 컴포넌트 설정
    ShieldMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShieldMesh"));
    ShieldMeshComp->SetupAttachment(GetMesh(), TEXT("Weapon_HitCenter")); // 소켓 부착
    ShieldMeshComp->SetVisibility(false);
    ShieldMeshComp->SetCollisionProfileName(TEXT("NoCollision")); // 기본은 충돌 없음

    // (임시) 큐브 메쉬 할당
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        ShieldMeshComp->SetStaticMesh(CubeMesh.Object);
        ShieldMeshComp->SetRelativeScale3D(FVector(0.1f, 2.5f, 3.5f));
        ShieldMeshComp->SetRelativeLocation(FVector(50.0f, 0.0f, 0.0f));
    }

    // 4. UI 및 VFX 설정
    ShieldBarWidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("ShieldBarWidget"));
    ShieldBarWidgetComp->SetupAttachment(ShieldMeshComp);
    ShieldBarWidgetComp->SetWidgetSpace(EWidgetSpace::World);
    ShieldBarWidgetComp->SetDrawSize(FVector2D(100.0f, 15.0f));
    ShieldBarWidgetComp->SetVisibility(false);

    SmashVFXScale = FVector(1.0f, 1.0f, 1.0f);
}

void APaladinCharacter::BeginPlay()
{
    Super::BeginPlay();

    // [Server] 방패 상태 초기화
    if (HasAuthority())
    {
        CurrentShieldHP = MaxShieldHP;
        bIsShieldBroken = false;
    }

    // [Physics] 캐릭터 캡슐과 방패 간의 충돌 무시 (밀려남 방지)
    if (GetCapsuleComponent() && ShieldMeshComp)
    {
        GetCapsuleComponent()->IgnoreComponentWhenMoving(ShieldMeshComp, true);
    }

    // [Optimization] 무기 메쉬 및 소켓 캐싱
    TArray<UStaticMeshComponent*> StaticComps;
    GetComponents<UStaticMeshComponent>(StaticComps);
    for (UStaticMeshComponent* Comp : StaticComps)
    {
        if (Comp && Comp->DoesSocketExist(TEXT("Weapon_HitCenter")))
        {
            CachedWeaponMesh = Comp;
            break;
        }
    }

    // [VFX] 무기 트레일 컴포넌트 생성 및 초기화 (꺼둠)
    if (CachedWeaponMesh && WeaponTrailVFX)
    {
        WeaponTrailComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
            WeaponTrailVFX,
            CachedWeaponMesh,
            NAME_None,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            EAttachLocation::SnapToTarget,
            false // Auto Destroy Off
        );
        if (WeaponTrailComp) WeaponTrailComp->Deactivate();
    }

    // 시작 시 방패 비활성화
    SetShieldActive(false);

    // [Data] 스킬 데이터 테이블 캐싱
    if (SkillDataTable)
    {
        TArray<FName> RowNames = SkillDataTable->GetRowNames();
        static const FString ContextString(TEXT("SkillDataCacheContext"));

        for (const FName& RowName : RowNames)
        {
            FSkillData* Data = SkillDataTable->FindRow<FSkillData>(RowName, ContextString);
            if (Data) SkillDataCache.Add(RowName, Data);
        }
    }
}

void APaladinCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(APaladinCharacter, bIsGuarding);
    DOREPLIFETIME(APaladinCharacter, CurrentShieldHP);
    DOREPLIFETIME(APaladinCharacter, bIsShieldBroken);
}

void APaladinCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        // 직업 능력 (방어 해제)
        if (JobAbilityAction)
        {
            EIC->BindAction(JobAbilityAction, ETriggerEvent::Completed, this, &APaladinCharacter::EndJobAbility);
        }
    }
}

// ====================================================================================
//  [Section 2] 상태 관리 및 이벤트 (State & Events)
// ====================================================================================

void APaladinCharacter::OnDeath()
{
    // 사망 시 모든 전투 상태 강제 해제
    SetShieldActive(false);
    bIsGuarding = false;
    StopAnimMontage();

    Super::OnDeath();
}

void APaladinCharacter::Landed(const FHitResult& Hit)
{
    Super::Landed(Hit);

    // [Smash Skill] 분쇄 스킬 사용 후 착지 시 상태 초기화
    if (bIsSmashing)
    {
        bIsSmashing = false;
        // 참고: 데미지 로직은 PerformSmashDamage 타이머가 처리함
    }
}

// ====================================================================================
//  [Section 3] 기본 공격 및 콤보 (Combat: Basic Attack)
// ====================================================================================

void APaladinCharacter::Attack()
{
    if (!CanAttack()) return;
    if (bIsGuarding) return; // 방어 중 공격 불가

    // 콤보 인덱스에 따른 스킬명 결정
    FName SkillName = (CurrentComboIndex == 0) ? TEXT("Attack_A") : TEXT("Attack_B");
    ProcessSkill(SkillName);

    StartAttackCooldown();

    // 콤보 로직 업데이트
    CurrentComboIndex++;
    if (CurrentComboIndex > 1) CurrentComboIndex = 0;

    GetWorldTimerManager().ClearTimer(ComboResetTimer);
    GetWorldTimerManager().SetTimer(ComboResetTimer, this, &APaladinCharacter::ResetCombo, 1.5f, false);
}

void APaladinCharacter::ResetCombo()
{
    CurrentComboIndex = 0;
}

// ------------------------------------------------------------------------------------
// Melee Trace Logic (근접 공격 판정)
// ------------------------------------------------------------------------------------

void APaladinCharacter::StartMeleeTrace()
{
    SwingDamagedActors.Empty();

    // 무기 위치 파악
    USceneComponent* WeaponComp = (CachedWeaponMesh) ? (USceneComponent*)CachedWeaponMesh : GetMesh();
    if (WeaponComp) LastHammerLocation = WeaponComp->GetSocketLocation(TEXT("Weapon_HitCenter"));
    else LastHammerLocation = GetActorLocation() + GetActorForwardVector() * 100.f;

    // 트레일 켜기
    Multicast_SetTrailActive(true);

    // 판정 루프 시작 (0.015초 간격)
    GetWorldTimerManager().SetTimer(HitLoopTimerHandle, this, &APaladinCharacter::PerformMeleeTrace, 0.015f, true);
    
    // 자동 종료 타이머
    FTimerHandle StopTimer;
    GetWorldTimerManager().SetTimer(StopTimer, this, &APaladinCharacter::StopMeleeTrace, HitDuration, false);
}

void APaladinCharacter::PerformMeleeTrace()
{
    USceneComponent* WeaponComp = (CachedWeaponMesh) ? (USceneComponent*)CachedWeaponMesh : GetMesh();
    FVector CurrentLoc;
    FQuat CurrentRot;

    if (WeaponComp && WeaponComp->DoesSocketExist(TEXT("Weapon_HitCenter")))
    {
        CurrentLoc = WeaponComp->GetSocketLocation(TEXT("Weapon_HitCenter"));
        CurrentRot = WeaponComp->GetSocketQuaternion(TEXT("Weapon_HitCenter"));
    }
    else
    {
        CurrentLoc = GetActorLocation() + GetActorForwardVector() * 100.f;
        CurrentRot = GetActorQuat();
    }

    // Box Sweep 실행
    TArray<FHitResult> HitResults;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    bool bHit = GetWorld()->SweepMultiByChannel(
        HitResults, LastHammerLocation, CurrentLoc, CurrentRot,
        ECC_Pawn, FCollisionShape::MakeBox(HammerHitBoxSize), Params
    );

    if (bHit)
    {
        for (const FHitResult& Hit : HitResults)
        {
            AActor* HitActor = Hit.GetActor();
            
            // 아군(Player)은 타격 제외
            if (!HitActor || HitActor == this || HitActor->ActorHasTag(TEXT("Player"))) continue;

            // 중복 타격 방지
            if (!SwingDamagedActors.Contains(HitActor))
            {
                UGameplayStatics::ApplyDamage(HitActor, CurrentSkillDamage, GetController(), this, UDamageType::StaticClass());
                SwingDamagedActors.Add(HitActor);

                // 타격감 (Hit Stop)
                Client_TriggerHitStop();
            }
        }
    }
    LastHammerLocation = CurrentLoc;
}

void APaladinCharacter::StopMeleeTrace()
{
    GetWorldTimerManager().ClearTimer(HitLoopTimerHandle);
    SwingDamagedActors.Empty();
    Multicast_SetTrailActive(false);
}

// ====================================================================================
//  [Section 4] 타격감 및 역경직 (Feel: Hit Stop)
// ====================================================================================

void APaladinCharacter::Client_TriggerHitStop_Implementation()
{
    if (!IsLocallyControlled()) return;

    // 1. 시간 느리게
    CustomTimeDilation = HitStopTimeDilation;

    // 2. 복구 타이머
    GetWorldTimerManager().ClearTimer(HitStopTimer);
    
    float ActualDelay = HitStopDuration * HitStopTimeDilation;
    GetWorldTimerManager().SetTimer(HitStopTimer, this, &APaladinCharacter::RestoreTimeDilation, ActualDelay, false);

    // 3. 카메라 흔들림
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (HitShakeClass) PC->ClientStartCameraShake(HitShakeClass);
    }
}

void APaladinCharacter::RestoreTimeDilation()
{
    CustomTimeDilation = 1.0f;
}

// ====================================================================================
//  [Section 5] 스킬 시스템 (Combat: Skill System)
// ====================================================================================

void APaladinCharacter::ProcessSkill(FName SkillRowName)
{
    if (!CanAttack()) return;

    // 1. 데이터 조회
    FSkillData** FoundData = SkillDataCache.Find(SkillRowName);
    FSkillData* Data = (FoundData) ? *FoundData : nullptr;

    if (Data)
    {
        if (!IsSkillUnlocked(Data->RequiredStage)) return;

        // 스탯 로드
        CurrentSkillDamage = Data->Damage;
        CurrentActionDelay = Data->ActionDelay;

        // 쿨타임 체크
        if (SkillTimers.Contains(SkillRowName) && GetWorldTimerManager().IsTimerActive(SkillTimers[SkillRowName])) return;

        // 몽타주 재생 (Client/Server 공통)
        if (Data->SkillMontage) PlayActionMontage(Data->SkillMontage);

        // 쿨타임 등록 & UI 갱신
        if (Data->Cooldown > 0.0f)
        {
            FTimerHandle& Handle = SkillTimers.FindOrAdd(SkillRowName);
            GetWorldTimerManager().SetTimer(Handle, Data->Cooldown, false);

            int32 SkillIdx = (SkillRowName == FName("Skill1")) ? 1 : 0;
            if (SkillIdx > 0) TriggerSkillCooldown(SkillIdx, Data->Cooldown);
        }

        // [Server Logic] 실제 데미지 및 물리 처리
        if (HasAuthority())
        {
            if (Data->SkillMontage) Server_PlayMontage(Data->SkillMontage);

            // ----------------------------------------------------
            // [Case A] Skill1: 분쇄 (Smash - Jump Attack)
            // ----------------------------------------------------
            if (SkillRowName == FName("Skill1"))
            {
                bHasDealtSmashDamage = false; 

                // 평타 캔슬 (중복 데미지 방지)
                GetWorldTimerManager().ClearTimer(AttackHitTimerHandle);
                GetWorldTimerManager().ClearTimer(HitLoopTimerHandle);
                StopMeleeTrace();

                Multicast_SetTrailActive(true);

                // 1. 도약 물리 계산
                float JumpDuration = (CurrentActionDelay > 0.f) ? CurrentActionDelay : 1.0f;
                float Gravity = 980.0f;
                if (UCharacterMovementComponent* CharMove = GetCharacterMovement())
                {
                    Gravity *= CharMove->GravityScale;
                    CharMove->SetMovementMode(MOVE_Falling);
                }
                float RequiredLaunchZ = (JumpDuration * Gravity) / 2.0f;

                // 2. 캐릭터 발사
                FVector LaunchDir = (GetActorForwardVector() * 800.0f) + (FVector::UpVector * RequiredLaunchZ);
                LaunchCharacter(LaunchDir, true, true);

                // 3. 착지 시 데미지 타이머 설정
                float FinalDamage = Data->Damage;
                bIsSmashing = true; 

                if (CurrentActionDelay > 0.0f)
                {
                    FTimerHandle SmashTimer;
                    FTimerDelegate TimerDel;
                    TimerDel.BindUFunction(this, FName("PerformSmashDamage"), FinalDamage);
                    GetWorldTimerManager().SetTimer(SmashTimer, TimerDel, CurrentActionDelay, false);
                }
                else
                {
                    PerformSmashDamage(FinalDamage);
                }
            }
            // ----------------------------------------------------
            // [Case B] Normal Attack: 일반 공격
            // ----------------------------------------------------
            else
            {
                GetWorldTimerManager().ClearTimer(AttackHitTimerHandle);
                GetWorldTimerManager().ClearTimer(HitLoopTimerHandle);

                if (CurrentActionDelay > 0.0f)
                {
                    GetWorldTimerManager().SetTimer(AttackHitTimerHandle, this, &APaladinCharacter::StartMeleeTrace, CurrentActionDelay, false);
                }
                else
                {
                    StartMeleeTrace();
                }
            }
        }
        else
        {
            // [Client] 서버에 실행 요청
            Server_ProcessSkill(SkillRowName);
        }
    }
}

void APaladinCharacter::Server_ProcessSkill_Implementation(FName SkillRowName)
{
    ProcessSkill(SkillRowName);
}

// ------------------------------------------------------------------------------------
// Skill 1: 분쇄 (Smash)
// ------------------------------------------------------------------------------------

void APaladinCharacter::Skill1()
{
    if (bIsDead || bIsGuarding) return;
    ProcessSkill(FName("Skill1"));
}

void APaladinCharacter::PerformSmashDamage(float SmashingDamage)
{
    if (!HasAuthority()) return;
    if (bHasDealtSmashDamage) return; // 중복 방지

    bHasDealtSmashDamage = true;
    Multicast_SetTrailActive(false);

    FVector ImpactLocation = GetActorLocation();
    Multicast_PlaySmashVFX(ImpactLocation);

    // 디버그 구체
    float AttackRadius = 350.0f;
    DrawDebugSphere(GetWorld(), ImpactLocation, AttackRadius, 12, FColor::Red, false, 3.0f);

    // 광역 판정 (Sphere Sweep)
    TArray<FHitResult> HitResults;
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(AttackRadius);
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    bool bHit = GetWorld()->SweepMultiByChannel(
        HitResults, ImpactLocation, ImpactLocation, FQuat::Identity, ECC_Pawn, SphereShape, Params
    );

    TSet<AActor*> HitActors;

    if (bHit)
    {
        for (const FHitResult& Hit : HitResults)
        {
            AActor* Victim = Hit.GetActor();
            if (!Victim || Victim == this || HitActors.Contains(Victim)) continue;
            HitActors.Add(Victim);

            // 1. 넉백 (적 & 아군 모두 적용될 수 있음, 필요시 필터링)
            ACharacter* VictimChar = Cast<ACharacter>(Victim);
            if (VictimChar)
            {
                FVector KnockbackDir = (Victim->GetActorLocation() - ImpactLocation).GetSafeNormal();
                KnockbackDir.Z = 0.0f; 
                KnockbackDir.Normalize();

                FVector LaunchForce = (KnockbackDir * 1000.0f) + FVector(0.0f, 0.0f, 400.0f);

                if (VictimChar->GetCharacterMovement())
                {
                    VictimChar->GetCharacterMovement()->StopMovementImmediately();
                    VictimChar->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
                }
                VictimChar->LaunchCharacter(LaunchForce, true, true);
            }

            // 2. 데미지 (적군만)
            if (!Victim->ActorHasTag(TEXT("Player")))
            {
                UGameplayStatics::ApplyDamage(Victim, SmashingDamage, GetController(), this, UDamageType::StaticClass());
            }
        }
    }
}

void APaladinCharacter::Multicast_PlaySmashVFX_Implementation(FVector Location)
{
    if (SmashImpactVFX)
    {
        FVector SpawnLocation = Location;
        SpawnLocation.Z -= GetCapsuleComponent()->GetScaledCapsuleHalfHeight(); // 바닥에서 터지게

        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(), SmashImpactVFX, SpawnLocation, FRotator::ZeroRotator,
            SmashVFXScale, true, true, ENCPoolMethod::None, true
        );
    }
}

// ------------------------------------------------------------------------------------
// Skill 2: 망치 소환 (Hammer Spawn)
// ------------------------------------------------------------------------------------

void APaladinCharacter::Skill2()
{
    if (!CanAttack()) return;
    if (bIsDead || bIsGuarding) return;

    // 로컬 쿨타임 체크
    if (GetWorld()->GetTimeSeconds() < Skill2ReadyTime) return;

    // 클라이언트: 몽타주 및 VFX 선행 재생
    if (SkillDataTable)
    {
        static const FString ContextString(TEXT("PaladinSkill2_Client"));
        FSkillData* Data = SkillDataTable->FindRow<FSkillData>(TEXT("Skill2"), ContextString);

        if (Data && IsSkillUnlocked(Data->RequiredStage))
        {
            if (Data->SkillMontage) PlayActionMontage(Data->SkillMontage);

            if (Skill2CastVFX)
            {
                FVector SpawnLoc = GetActorLocation();
                SpawnLoc.Z -= GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
                UNiagaraFunctionLibrary::SpawnSystemAttached(
                    Skill2CastVFX, GetMesh(), TEXT("Hand_R_Root"),
                    FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true
                );
            }
            // 서버에 실제 소환 요청
            Server_Skill2();
        }
    }
}

void APaladinCharacter::Server_Skill2_Implementation()
{
    float FinalCooldown = 10.0f;
    float FinalDamage = 70.0f;
    float DelayTime = 0.0f;

    if (SkillDataTable)
    {
        static const FString ContextString(TEXT("PaladinSkill2"));
        FSkillData* Data = SkillDataTable->FindRow<FSkillData>(TEXT("Skill2"), ContextString);
        if (Data)
        {
            if (!IsSkillUnlocked(Data->RequiredStage)) return;

            FinalDamage = Data->Damage;
            if (Data->Cooldown > 0.0f) FinalCooldown = Data->Cooldown;
            DelayTime = Data->ActionDelay;

            // 타 클라이언트 동기화
            if (Data->SkillMontage) Multicast_PlayMontage(Data->SkillMontage);
        }
    }

    // 쿨타임 및 VFX 처리
    Skill2ReadyTime = GetWorld()->GetTimeSeconds() + FinalCooldown;
    TriggerSkillCooldown(2, FinalCooldown);
    Multicast_PlaySkill2CastVFX();

    // 딜레이 후 망치 소환
    if (DelayTime > 0.0f)
    {
        FTimerHandle SpawnTimerHandle;
        FTimerDelegate TimerDel;
        TimerDel.BindUFunction(this, FName("SpawnSkill2Hammer"), FinalDamage);
        GetWorldTimerManager().SetTimer(SpawnTimerHandle, TimerDel, DelayTime, false);
    }
    else
    {
        SpawnSkill2Hammer(FinalDamage);
    }
}

void APaladinCharacter::SpawnSkill2Hammer(float FinalDamage)
{
    if (!HasAuthority() || !Skill2HammerClass) return;

    FVector MyLoc = GetActorLocation();
    FVector Forward = GetActorForwardVector();

    // 위치 계산 (전방 8m, 상공 10m)
    FVector IdealLoc = MyLoc + (Forward * 800.0f) + FVector(0.f, 0.f, 1000.f);

    // 벽/천장 충돌 보정
    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, MyLoc, IdealLoc, ECC_Visibility, QueryParams);
    
    FVector FinalSpawnLoc = bHit ? (HitResult.Location - (HitResult.ImpactNormal * 50.0f)) : IdealLoc;

    // 디버그
    DrawDebugSphere(GetWorld(), FinalSpawnLoc, 30.0f, 12, FColor::Red, false, 3.0f);

    // 소환
    FRotator SpawnRot = FRotator(0.0f, GetActorRotation().Yaw, 0.0f);
    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.Instigator = this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    APaladinSkill2Hammer* Hammer = GetWorld()->SpawnActor<APaladinSkill2Hammer>(
        Skill2HammerClass, FinalSpawnLoc, SpawnRot, Params
    );

    if (Hammer)
    {
        Hammer->InitializeHammer(FinalDamage, this);
    }
}

void APaladinCharacter::Multicast_PlaySkill2CastVFX_Implementation()
{
    if (IsLocallyControlled()) return; // 주인은 이미 재생함

    if (Skill2CastVFX)
    {
        UNiagaraFunctionLibrary::SpawnSystemAttached(
            Skill2CastVFX, GetMesh(), TEXT("Hand_R_Root"),
            FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true
        );
    }
}

// ====================================================================================
//  [Section 6] 직업 능력 - 방어 (Job Ability: Shield)
// ====================================================================================

void APaladinCharacter::JobAbility()
{
    if (!CanAttack()) return;
    if (!IsSkillUnlocked(1)) return;
    if (bIsShieldBroken || CurrentShieldHP <= 0.0f) return;

    // 데이터 조회
    UAnimMontage* MontageToPlay = nullptr;
    float ActivationDelay = 0.0f;
    
    FSkillData** FoundData = SkillDataCache.Find(FName("JobAbility"));
    if (FoundData && *FoundData)
    {
        MontageToPlay = (*FoundData)->SkillMontage;
        ActivationDelay = (*FoundData)->ActionDelay;
    }

    // 1. 애니메이션
    if (MontageToPlay)
    {
        PlayActionMontage(MontageToPlay);
        Server_PlayMontage(MontageToPlay);
    }

    // 2. 방패 활성화 (딜레이 적용)
    if (ActivationDelay > 0.0f)
    {
        GetWorldTimerManager().SetTimer(ShieldActivationTimer, this, &APaladinCharacter::ActivateGuard, ActivationDelay, false);
    }
    else
    {
        ActivateGuard();
    }
}

void APaladinCharacter::EndJobAbility()
{
    // 버튼 뗄 때 방어 해제
    GetWorldTimerManager().ClearTimer(ShieldActivationTimer);
    Server_SetGuard(false);
}

void APaladinCharacter::ActivateGuard()
{
    Server_SetGuard(true);
}

void APaladinCharacter::Server_SetGuard_Implementation(bool bNewGuarding)
{
    if (bIsShieldBroken && bNewGuarding) return;

    bIsGuarding = bNewGuarding;

    if (bIsGuarding)
    {
        // 방어 시작: 회복 타이머 중단
        GetWorldTimerManager().ClearTimer(ShieldRegenTimer);
    }
    else
    {
        // 방어 해제: 몽타주 종료 및 회복 시작
        Multicast_StopMontage(0.25f);

        if (!GetWorldTimerManager().IsTimerActive(ShieldRegenTimer))
        {
            GetWorldTimerManager().SetTimer(ShieldRegenTimer, this, &APaladinCharacter::RegenShield, 0.1f, true, ShieldRegenDelay);
        }
    }
    OnRep_IsGuarding(); // 서버에서도 상태 반영 함수 호출
}

void APaladinCharacter::OnRep_IsGuarding()
{
    SetShieldActive(bIsGuarding);
}

void APaladinCharacter::SetShieldActive(bool bActive)
{
    if (!ShieldMeshComp) return;

    ShieldMeshComp->SetVisibility(bActive);

    // UI는 본인 화면에만 표시
    if (ShieldBarWidgetComp)
    {
        ShieldBarWidgetComp->SetVisibility(bActive && IsLocallyControlled());
    }

    if (bActive)
    {
        // [ON] 물리 충돌 활성화
        ShieldMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        ShieldMeshComp->SetCollisionObjectType(ECC_WorldDynamic);
        ShieldMeshComp->SetCollisionProfileName(TEXT("Custom"));

        ShieldMeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);
        ShieldMeshComp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
        
        // 투사체 처리 (아군은 통과, 적군은 차단)
        ShieldMeshComp->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
        ShieldMeshComp->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
        ShieldMeshComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    }
    else
    {
        // [OFF] 충돌 끄기
        ShieldMeshComp->SetCollisionProfileName(TEXT("NoCollision"));
        ShieldMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

void APaladinCharacter::OnShieldBroken()
{
    bIsShieldBroken = true;
    CurrentShieldHP = 0.0f;

    Server_SetGuard(false);
    GetWorldTimerManager().ClearTimer(ShieldRegenTimer);

    // 쿨타임 데이터 조회
    float BrokenCooldown = 5.0f;
    FSkillData** FoundData = SkillDataCache.Find(FName("JobAbility"));
    if (FoundData && (*FoundData)->Cooldown > 0.0f)
    {
        BrokenCooldown = (*FoundData)->Cooldown;
    }

    // 복구 타이머 및 UI 알림
    GetWorldTimerManager().SetTimer(ShieldBrokenTimerHandle, this, &APaladinCharacter::RestoreShieldAfterCooldown, BrokenCooldown, false);
    Client_TriggerSkillCooldown(3, BrokenCooldown);

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Shield Broken!"));
}

void APaladinCharacter::RestoreShieldAfterCooldown()
{
    GetWorldTimerManager().ClearTimer(ShieldActivationTimer);

    CurrentShieldHP = MaxShieldHP;
    bIsShieldBroken = false;

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Blue, TEXT("Shield Restored!"));
}

void APaladinCharacter::RegenShield()
{
    if (bIsGuarding) return;

    CurrentShieldHP += ShieldRegenRate * 0.1f;

    if (CurrentShieldHP >= MaxShieldHP)
    {
        CurrentShieldHP = MaxShieldHP;
        bIsShieldBroken = false;
        GetWorldTimerManager().ClearTimer(ShieldRegenTimer);
    }
    else if (bIsShieldBroken && CurrentShieldHP > MaxShieldHP * 0.3f)
    {
        // 30% 복구 시 사용 가능
        bIsShieldBroken = false;
    }
}

// ====================================================================================
//  [Section 7] 데미지 처리 및 네트워크 유틸 (Damage & Network)
// ====================================================================================

float APaladinCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    float ActualDamage = DamageAmount;

    // [Defense Logic] 방어 중일 때 전방 공격 차단
    if (HasAuthority() && bIsGuarding && !bIsShieldBroken && DamageCauser)
    {
        FVector DirectionFromAttacker = (GetActorLocation()) - DamageCauser->GetActorLocation();
     
        if (IsBlockingDirection(DirectionFromAttacker))
        {
            ActualDamage = 0.0f; // 본체 데미지 무효화
            CurrentShieldHP -= DamageAmount;

            if (CurrentShieldHP <= 0.0f)
            {
                OnShieldBroken();
            }
        }
    }

    return Super::TakeDamage(ActualDamage, DamageEvent, EventInstigator, DamageCauser);
}

bool APaladinCharacter::IsBlockingDirection(FVector IncomingDirection) const
{
    if (!bIsGuarding || bIsShieldBroken) return false;

    // 내적 계산: 서로 마주보는 방향일 때(값이 음수) 방어 성공
    float DotResult = FVector::DotProduct(GetActorForwardVector(), IncomingDirection.GetSafeNormal());
    return DotResult < -0.2f;
}

void APaladinCharacter::ConsumeShield(float Amount)
{
    if (!HasAuthority()) return;

    CurrentShieldHP -= Amount;
    if (CurrentShieldHP <= 0.0f)
    {
        OnShieldBroken();
    }
}

void APaladinCharacter::Server_PlayMontage_Implementation(UAnimMontage* MontageToPlay)
{
    Multicast_PlayMontage(MontageToPlay);
}

void APaladinCharacter::Multicast_PlayMontage_Implementation(UAnimMontage* MontageToPlay)
{
    if (MontageToPlay && !IsLocallyControlled())
    {
        PlayAnimMontage(MontageToPlay);
    }
}

void APaladinCharacter::Multicast_StopMontage_Implementation(float BlendOutTime)
{
    if (IsDead()) return;

    UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
    if (AnimInstance && AnimInstance->GetCurrentActiveMontage())
    {
        if (AnimInstance->GetCurrentActiveMontage() == DeathMontage) return;
        AnimInstance->Montage_Stop(BlendOutTime);
    }
}

void APaladinCharacter::Multicast_SetTrailActive_Implementation(bool bActive)
{
    if (WeaponTrailComp)
    {
        if (bActive) WeaponTrailComp->Activate(true);
        else WeaponTrailComp->Deactivate();
    }
}