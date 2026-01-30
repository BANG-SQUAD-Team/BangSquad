#include "Project_Bang_Squad/Character/PaladinCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/WidgetComponent.h"
#include "EnhancedInputComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/DataTable.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Engine/DamageEvents.h"
#include "DrawDebugHelpers.h" 
#include "NiagaraFunctionLibrary.h"
#include "Components/CapsuleComponent.h"

// ====================================================================================
//  섹션 1: 생성자 및 초기화 (Constructor & Initialization)
// ====================================================================================

APaladinCharacter::APaladinCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    
    bReplicates = true;
    SetReplicateMovement(true);
    
    // 1. 공격 및 이동 설정
    AttackCooldownTime = 1.f;
    bUseControllerRotationYaw = true;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;
    
        // CharacterMovement 컴포넌트 설정 (한 번만 가져와서 사용)
        if (UCharacterMovementComponent* CharMove = GetCharacterMovement())
        {
            CharMove->MaxWalkSpeed = 550.f;
            CharMove->bOrientRotationToMovement = false;
            CharMove->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
        }
    
    // 2. 카메라 설정
    if (SpringArm) 
    {
       SpringArm->TargetOffset = FVector(0.0f, 0.0f, 150.0f); 
       SpringArm->ProbeSize = 12.0f; 
    }
    
    // 3. 방패 컴포넌트 (초기화 및 부착)
    ShieldMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShieldMesh"));
    ShieldMeshComp->SetupAttachment(GetMesh(), TEXT("Weapon_HitCenter"));
    ShieldMeshComp->SetVisibility(false);
    ShieldMeshComp->SetCollisionProfileName(TEXT("NoCollision")); // 기본은 충돌 없음
    
    // 임시 메쉬 할당 (Cube)
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        ShieldMeshComp->SetStaticMesh(CubeMesh.Object);
        ShieldMeshComp->SetRelativeScale3D(FVector(0.1f, 2.5f, 3.5f)); 
        ShieldMeshComp->SetRelativeLocation(FVector(50.0f, 0.0f, 0.0f));
    }

    // 4. 방패 체력바 UI
    ShieldBarWidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("ShieldBarWidget"));
    ShieldBarWidgetComp->SetupAttachment(ShieldMeshComp);
    ShieldBarWidgetComp->SetWidgetSpace(EWidgetSpace::World);
    ShieldBarWidgetComp->SetDrawSize(FVector2D(100.0f, 15.0f));
    ShieldBarWidgetComp->SetVisibility(false);
    
    // 🔥 나이아가라 이펙트 크기 기본값 설정 (1.0배)
    SmashVFXScale = FVector(1.0f, 1.0f, 1.0f);
}


void APaladinCharacter::BeginPlay()
{
    Super::BeginPlay();
    
    // [서버] 방패 체력 초기화
    if (HasAuthority())
    {
        CurrentShieldHP = MaxShieldHP;
        bIsShieldBroken = false;
    }
    
    // [물리] 내 몸(Capsule)과 방패가 서로 밀어내지 않도록 무시 설정
    if (GetCapsuleComponent() && ShieldMeshComp)
    {
        GetCapsuleComponent()->IgnoreComponentWhenMoving(ShieldMeshComp, true);
    }
    
    // [최적화] 무기 메쉬 컴포넌트 미리 찾아서 캐싱 (매번 FindComponent 방지)
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
    
    // 시작 시 방패 비활성화 (확실하게 끄기)
    SetShieldActive(false);
    
    // 🔥 [추가] 데이터 테이블 캐싱 (시작할 때 한 번만 수행)
    if (SkillDataTable)
    {
        // 1. 테이블에 있는 모든 행(Row)의 이름을 가져옴
        TArray<FName> RowNames = SkillDataTable->GetRowNames();

        for (const FName& RowName : RowNames)
        {
            // 2. 이름으로 데이터 찾기
            static const FString ContextString(TEXT("SkillDataCacheContext"));
            FSkillData* Data = SkillDataTable->FindRow<FSkillData>(RowName, ContextString);

            // 3. 찾았으면 내 주머니(Map)에 저장
            if (Data)
            {
                SkillDataCache.Add(RowName, Data);
            }
        }
    }
}

void APaladinCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    
    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        // 직업 능력 (방어) 키 바인딩
        if (JobAbilityAction)
        {
            EIC->BindAction(JobAbilityAction, ETriggerEvent::Completed, this, &APaladinCharacter::EndJobAbility);
        }
    }
}

void APaladinCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // 리플리케이션 변수 등록
    DOREPLIFETIME(APaladinCharacter, bIsGuarding);
    DOREPLIFETIME(APaladinCharacter, CurrentShieldHP);
    DOREPLIFETIME(APaladinCharacter, bIsShieldBroken);
}

// ====================================================================================
//  섹션 2: 상태 변경 및 이벤트 오버라이드 (State & Event Overrides)
// ====================================================================================

void APaladinCharacter::OnDeath()
{
    // 사망 시 모든 전투 상태 해제
    SetShieldActive(false);
    bIsGuarding = false;
    StopAnimMontage();
    
    Super::OnDeath();
}

void APaladinCharacter::Landed(const FHitResult& Hit)
{
    Super::Landed(Hit);
    
    // 분쇄 스킬(점프) 사용 후 착지했을 때 처리
    if (bIsSmashing)
    {
        bIsSmashing = false; // 상태 초기화
        
        // *주의: 데미지 로직은 PerformSmashDamage 타이머가 처리하므로 여기엔 절대 넣지 않음.
        // 필요하다면 착지 이펙트(VFX)나 사운드만 재생.
    }
}

// ====================================================================================
//  섹션 3: 기본 공격 및 콤보 (Basic Attack & Combo System)
// ====================================================================================

void APaladinCharacter::Attack()
{
    if (!CanAttack()) return;
    if (bIsGuarding) return; // 방어 중 공격 불가
    
    // 콤보에 따른 스킬 이름 결정
    FName SkillName;
    if (CurrentComboIndex == 0) SkillName = TEXT("Attack_A");
    else SkillName = TEXT("Attack_B");
    
    ProcessSkill(SkillName);
    
    StartAttackCooldown();
    
    
    // 콤보 카운트 증가 및 리셋 타이머 설정
    CurrentComboIndex++;
    if (CurrentComboIndex > 1) CurrentComboIndex = 0;
    
    GetWorldTimerManager().ClearTimer(ComboResetTimer);
    GetWorldTimerManager().SetTimer(ComboResetTimer, this, &APaladinCharacter::ResetCombo, 1.5f, false);
}

void APaladinCharacter::ResetCombo()
{
    CurrentComboIndex = 0;
}

// [평타 판정 1단계] 궤적 검사 시작
void APaladinCharacter::StartMeleeTrace()
{
    SwingDamagedActors.Empty();

    // 무기 위치 찾기 (캐싱된 것 우선 사용)
    USceneComponent* WeaponComp = GetMesh();
    if (CachedWeaponMesh) WeaponComp = CachedWeaponMesh;
    else 
    {
        // 캐싱 실패 시 재검색
        TArray<UStaticMeshComponent*> StaticComps;
        GetComponents<UStaticMeshComponent>(StaticComps);
        for (UStaticMeshComponent* Comp : StaticComps)
        {
            if (Comp && Comp->DoesSocketExist(TEXT("Weapon_HitCenter"))) { WeaponComp = Comp; break; }
        }
    }

    if (WeaponComp) LastHammerLocation = WeaponComp->GetSocketLocation(TEXT("Weapon_HitCenter"));
    else LastHammerLocation = GetActorLocation() + GetActorForwardVector() * 100.f;

    // 반복 검사 타이머 시작 (0.015초마다 궤적 스윕)
    GetWorldTimerManager().SetTimer(HitLoopTimerHandle, this, &APaladinCharacter::PerformMeleeTrace, 0.015f, true);
    
    // 일정 시간 후 판정 종료
    FTimerHandle StopTimer;
    GetWorldTimerManager().SetTimer(StopTimer, this, &APaladinCharacter::StopMeleeTrace, HitDuration, false);
}

// [평타 판정 2단계] 궤적 스윕 및 데미지 적용
void APaladinCharacter::PerformMeleeTrace()
{
    // 무기 컴포넌트 확보
    USceneComponent* WeaponComp = GetMesh();
    if (CachedWeaponMesh) WeaponComp = CachedWeaponMesh;

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

    // 스윕 파라미터
    TArray<FHitResult> HitResults;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    // 이전 위치 -> 현재 위치로 박스 스윕
    bool bHit = GetWorld()->SweepMultiByChannel(
        HitResults, LastHammerLocation, CurrentLoc, CurrentRot,
        ECC_Pawn, FCollisionShape::MakeBox(HammerHitBoxSize), Params
    );

    if (bHit)
    {
        for (const FHitResult& Hit : HitResults)
        {
            AActor* HitActor = Hit.GetActor();
            
            // 아군(Player)이면 무시하고 넘어감
            if (HitActor && HitActor->ActorHasTag(TEXT("Player")))
            {
                continue;
            }
            
            // 중복 피격 방지 (한 번 휘두를 때 한 번만 맞도록)
            if (HitActor && HitActor != this && !SwingDamagedActors.Contains(HitActor))
            {
                UGameplayStatics::ApplyDamage(HitActor, CurrentSkillDamage, GetController(), this, UDamageType::StaticClass());
                SwingDamagedActors.Add(HitActor);
                
                Client_TriggerHitStop();
            }
        }
    }
    LastHammerLocation = CurrentLoc;
}

// [평타 판정 3단계] 종료 및 초기화
void APaladinCharacter::StopMeleeTrace()
{
    GetWorldTimerManager().ClearTimer(HitLoopTimerHandle);
    SwingDamagedActors.Empty();
}

void APaladinCharacter::Client_TriggerHitStop_Implementation()
{
    // 여기서 시간을 멈추면 나에게만 보임
    
    // 1. 나 자신이 로컬 플레이어인지 확인
    if (!IsLocallyControlled()) return;
    // 2. 시간 조작 (Hit Stop)
    CustomTimeDilation = HitStopTimeDilation;
    // 3. 복구 타이머 설정
    GetWorldTimerManager().ClearTimer(HitStopTimer);
    GetWorldTimerManager().SetTimer(HitStopTimer, this, &APaladinCharacter::RestoreTimeDilation, HitStopDuration, false);
    
    // 여기서 카메라 흔들림을 넣어야 나만 화면이 흔들림
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (HitShakeClass)
        {
            PC->ClientStartCameraShake(HitShakeClass);
        }
    }
}

void APaladinCharacter::RestoreTimeDilation()
{
    // 시간을 원래 속도로 복구
    CustomTimeDilation = 1.0f;
}

// ====================================================================================
//  섹션 4: 스킬 시스템 (Skill System - ProcessSkill & Smash)
// ====================================================================================

void APaladinCharacter::Skill1()
{
    if (bIsDead || bIsGuarding) return;
    ProcessSkill(FName("Skill1"));
}

void APaladinCharacter::ProcessSkill(FName SkillRowName)
{
    if (!CanAttack()) return;
    
    // 캐시에서 데이터 꺼내오기
    FSkillData** FoundData = SkillDataCache.Find(SkillRowName);
    FSkillData* Data = (FoundData) ? *FoundData : nullptr;
    
    if (Data)
    {
        if (!IsSkillUnlocked(Data->RequiredStage)) return;
       
        // 데이터 테이블 값 로드
        CurrentSkillDamage = Data->Damage;
        CurrentActionDelay = Data->ActionDelay;
       
        // 쿨타임 중이면 실행 취소
        if (SkillTimers.Contains(SkillRowName) && GetWorldTimerManager().IsTimerActive(SkillTimers[SkillRowName])) return;
        
        // 몽타주 재생 (Client/Server 공통)
        if (Data->SkillMontage) PlayActionMontage(Data->SkillMontage);

        // 쿨타임 타이머 등록
        if (Data->Cooldown > 0.0f)
        {
            FTimerHandle& Handle = SkillTimers.FindOrAdd(SkillRowName);
            GetWorldTimerManager().SetTimer(Handle, Data->Cooldown, false);
        }
        
        // [서버 로직] 실제 판정 처리
        if (HasAuthority())
        {
            if (Data->SkillMontage) Server_PlayMontage(Data->SkillMontage);
            
            // ----------------------------------------------------
            // Case A: 스킬1 분쇄 (점프 공격)
            // ----------------------------------------------------
            if (SkillRowName == FName("Skill1"))
            {
                // [필수] "아직 데미지 안 줬음" 플래그 초기화
                bHasDealtSmashDamage = false; 

                // [중요] 혹시 실행 중일 평타 판정 강제 종료 (중복 데미지 방지)
                GetWorldTimerManager().ClearTimer(AttackHitTimerHandle);
                GetWorldTimerManager().ClearTimer(HitLoopTimerHandle);
                StopMeleeTrace();
                
                // 1. 점프 높이 계산 (체공 시간 기준 역산)
                float JumpDuration = CurrentActionDelay;
                if (JumpDuration <= 0.0f) JumpDuration = 1.0f;

                float Gravity = 980.0f;
                UCharacterMovementComponent* CharMove = GetCharacterMovement();
                if (CharMove) Gravity *= CharMove->GravityScale;
                float RequiredLaunchZ = (JumpDuration * Gravity) / 2.0f;

                // 2. 캐릭터 발사
                FVector LaunchDir = (GetActorForwardVector() * 800.0f) + (FVector::UpVector * RequiredLaunchZ);
                //무조건 날아가게 하기
                if (CharMove)
                {
                    CharMove->SetMovementMode(MOVE_Falling);
                }
                // 그 후에 발사
                LaunchCharacter(LaunchDir, true, true);
            
                // [핵심] 타이머 호출 시, 현재 데미지 값을 고정(Capture)해서 전달
                float FinalDamage = Data->Damage;
                
                if (CurrentActionDelay > 0.0f)
                {
                    FTimerHandle SmashTimer;
                    FTimerDelegate TimerDel;

                    // "PerformSmashDamage 함수를 실행할 때 FinalDamage 값을 인자로 넣어라"
                    TimerDel.BindUFunction(this, FName("PerformSmashDamage"), FinalDamage);

                    GetWorldTimerManager().SetTimer(SmashTimer, TimerDel, CurrentActionDelay, false);
                }
                else
                {
                    PerformSmashDamage(FinalDamage);
                }

                bIsSmashing = true; // 착지 로직 활성화
            }
            // ----------------------------------------------------
            // Case B: 일반 공격 (평타)
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
            // 클라이언트라면 서버에 요청
            Server_ProcessSkill(SkillRowName);
        }
    }
}

void APaladinCharacter::Server_ProcessSkill_Implementation(FName SkillRowName)
{
    ProcessSkill(SkillRowName);
}

void APaladinCharacter::PerformSmashDamage(float SmashingDamage)
{
    // 1. 서버 권한 체크
    if (!HasAuthority()) return;

    // 2. [중복 방지] 이미 데미지를 줬다면 즉시 종료
    if (bHasDealtSmashDamage) return;
    bHasDealtSmashDamage = true; // "데미지 줌" 체크
   
    // 3. 광역 데미지 로직
    FVector ImpactLocation = GetActorLocation(); 
    Multicast_PlaySmashVFX(ImpactLocation);
    float AttackRadius = 350.0f;
    DrawDebugSphere(GetWorld(), ImpactLocation, AttackRadius, 12, FColor::Red, false, 3.0f);
    
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
            
            if (Victim && Victim->ActorHasTag(TEXT("Player")))
            {
                continue;
            }
            
            // 중복되지 않은 대상에게만 데미지 적용
            if (Victim && !HitActors.Contains(Victim) && Victim != this)
            {
                HitActors.Add(Victim);
                // 파라미터로 받은 '고정된 데미지(SmashingDamage)' 사용
                UGameplayStatics::ApplyDamage(Victim, SmashingDamage, GetController(), this, UDamageType::StaticClass());
            }
        }
    }
}

void APaladinCharacter::Multicast_PlaySmashVFX_Implementation(FVector Location)
{
    if (SmashImpactVFX)
    {
        // 캐릭터 발밑(Z축 0)에서 터지게 조정
        FVector SpawnLocation = Location;
        SpawnLocation.Z -= GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(), 
            SmashImpactVFX, 
            SpawnLocation, 
            FRotator::ZeroRotator,
            SmashVFXScale,
            true,true, ENCPoolMethod::None, true
        );
    }
}


// ====================================================================================
//  섹션 5: 방어 시스템 (Job Ability - Shield Logic)
// ====================================================================================

void APaladinCharacter::JobAbility()
{
    if (!CanAttack()) return;
    
    if (!IsSkillUnlocked(1)) return;
    
    // 방패가 깨졌거나 HP 0이면 사용 불가
    if (bIsShieldBroken || CurrentShieldHP <= 0.0f) return;
    
    UAnimMontage* MontageToPlay = nullptr;
    float ActivationDelay = 0.0f;

    // 캐시에서 조회
    FSkillData** FoundData = SkillDataCache.Find(FName("JobAbility"));
    FSkillData* Data = (FoundData) ? *FoundData : nullptr;
    if (Data)
    {
        MontageToPlay = Data->SkillMontage;
        ActivationDelay = Data->ActionDelay;
    }
    
    // 1. 애니메이션 즉시 재생
    if (MontageToPlay)
    {
        PlayActionMontage(MontageToPlay);
        Server_PlayMontage(MontageToPlay);
    }
    
    // 2. 딜레이 후 방패 활성화
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
    // 버튼 떼면 타이머 취소 및 방어 해제
    GetWorldTimerManager().ClearTimer(ShieldActivationTimer);
    Server_SetGuard(false);
}

void APaladinCharacter::ActivateGuard()
{
    Server_SetGuard(true);
}

void APaladinCharacter::Server_SetGuard_Implementation(bool bNewGuarding)
{
    // 깨진 상태에서는 방어 불가
    if (bIsShieldBroken && bNewGuarding) return;
    
    bIsGuarding = bNewGuarding;
    
    if (bIsGuarding)
    {
        // 방어 시작: 회복 중단
        GetWorldTimerManager().ClearTimer(ShieldRegenTimer);
    }
    else
    {
        // 방어 해제: 몽타주 정지 및 회복 로직 시작
        Multicast_StopMontage(0.25f);
        
        bool bIsWaitingForRepair = GetWorldTimerManager().IsTimerActive(ShieldRegenTimer);
        if (!bIsWaitingForRepair && !GetWorldTimerManager().IsTimerActive(ShieldRegenTimer))
        {
            // 깨지지 않았다면 즉시 서서히 회복
            GetWorldTimerManager().SetTimer(ShieldRegenTimer, this, &APaladinCharacter::RegenShield, 0.1f, true, ShieldRegenDelay);
        }
    }
    OnRep_IsGuarding();
}

void APaladinCharacter::OnRep_IsGuarding()
{
    SetShieldActive(bIsGuarding);
}

// 방패의 물리적/시각적 활성화 처리
void APaladinCharacter::SetShieldActive(bool bActive)
{
    if (!ShieldMeshComp) return;

    ShieldMeshComp->SetVisibility(bActive);

    // 로컬 플레이어에게만 UI 표시
    if (ShieldBarWidgetComp) 
    {
        ShieldBarWidgetComp->SetVisibility(bActive && IsLocallyControlled());
    }

    if (bActive)
    {
        // [ON] 방패 켜기: 충돌 채널 설정
        ShieldMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        ShieldMeshComp->SetCollisionObjectType(ECC_WorldDynamic);
        ShieldMeshComp->SetCollisionProfileName(TEXT("Custom")); 

        ShieldMeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);
        ShieldMeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
        ShieldMeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);   

        // 아군 투사체는 통과, 적군은 차단
        ShieldMeshComp->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore); 
        ShieldMeshComp->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
        ShieldMeshComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    }
    else
    {
        // [OFF] 방패 끄기: 모든 충돌 제거
        ShieldMeshComp->SetCollisionProfileName(TEXT("NoCollision"));
        ShieldMeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);
        ShieldMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

// 방패 파괴 시 로직 (Cooling Down)
void APaladinCharacter::OnShieldBroken()
{
    bIsShieldBroken = true;
    CurrentShieldHP = 0.0f;
    
    Server_SetGuard(false); // 강제 해제
    
    GetWorldTimerManager().ClearTimer(ShieldRegenTimer);
    
    // 데이터 테이블에서 쿨타임 가져오기
    float BrokenCooldown = 5.0f;
    
    // 캐시에서 조회
    FSkillData** FoundData = SkillDataCache.Find(FName("JobAbility"));
    FSkillData* Data = (FoundData) ? *FoundData : nullptr;
    
    if (Data && Data->Cooldown > 0.0f)
    {
        BrokenCooldown = Data->Cooldown;
    }
    
    // 수리 대기 타이머 시작
    GetWorldTimerManager().SetTimer(ShieldBrokenTimerHandle, this, &APaladinCharacter::RestoreShieldAfterCooldown, BrokenCooldown, false);
    
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Shield is Broken! Waiting for cooldown..."));
}

void APaladinCharacter::RestoreShieldAfterCooldown()
{
    GetWorldTimerManager().ClearTimer(ShieldActivationTimer);
    
    // 방패 완전 복구
    CurrentShieldHP = MaxShieldHP;
    bIsShieldBroken = false;
    
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Blue, TEXT("Shield Fully Restored!"));
}

// 방패 서서히 회복 (Regeneration)
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
        // 30% 이상 차면 사용 가능 상태로 복구
        bIsShieldBroken = false;
    }
}

// ====================================================================================
//  섹션 6: 데미지 처리 (Damage Handling)
// ====================================================================================

float APaladinCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    float ActualDamage = DamageAmount;

    // [방어 로직] 서버 & 방어 중 & 방패 유효함
    if (HasAuthority() && bIsGuarding && !bIsShieldBroken && DamageCauser)
    {
        // 적이 앞쪽에 있는지 확인 (로컬 X 좌표가 양수면 앞)
        FVector LocalEnemyLoc = GetActorTransform().InverseTransformPosition(DamageCauser->GetActorLocation());
        
        if (LocalEnemyLoc.X > 100.0f)
        {
            // 방어 성공: 본체 데미지 0, 방패 체력 감소
            ActualDamage = 0.0f; 
            CurrentShieldHP -= DamageAmount;
            
            // 방패 파괴 체크
            if (CurrentShieldHP <= 0.0f)
            {
                OnShieldBroken();
            }
        }
    }

    // 최종 데미지 적용
    return Super::TakeDamage(ActualDamage, DamageEvent, EventInstigator, DamageCauser);
}

// ====================================================================================
//  섹션 7: 네트워크 및 유틸리티 (Network RPCs & Utils)
// ====================================================================================

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

// 1. 방향 판정 헬퍼 (내적 활용)
bool APaladinCharacter::IsBlockingDirection(FVector IncomingDirection) const
{
    // 방어 중이 아니거나 방패가 깨졌으면 못 막음
    if (!bIsGuarding || bIsShieldBroken) return false;

    // 내적(Dot Product) 계산
    // 내 정면(Forward)과 들어오는 힘(Direction)이 서로 마주봐야 함 (반대 방향)
    // 예: 바람이 남쪽(↓)으로 불 때, 나는 북쪽(↑)을 봐야 막아짐 -> 내적 값은 -1.0
    float DotResult = FVector::DotProduct(GetActorForwardVector(), IncomingDirection.GetSafeNormal());

    // -0.2 이하면 대략 전방 160도 정도 커버 (수치가 -1에 가까울수록 정면만 막음)
    return DotResult < -0.2f;
}

// 2. 방패 체력 소모 (서버 전용)
void APaladinCharacter::ConsumeShield(float Amount)
{
    if (!HasAuthority()) return; // 서버만 처리

    CurrentShieldHP -= Amount;
    

    // 깨짐 체크
    if (CurrentShieldHP <= 0.0f)
    {
        OnShieldBroken();
    }
}