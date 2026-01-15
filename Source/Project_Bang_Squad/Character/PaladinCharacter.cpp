#include "Project_Bang_Squad/Character/PaladinCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/WidgetComponent.h"
#include "EnhancedInputComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/DataTable.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DamageEvents.h"
#include "DrawDebugHelpers.h" 
#include "Components/CapsuleComponent.h"

APaladinCharacter::APaladinCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    
    bReplicates = true;
    SetReplicateMovement(true);
    
    // 2. 공격 쿨타임
    AttackCooldownTime = 1.f;
    
    // 3. 스트레이핑 모드
    bUseControllerRotationYaw = true;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
    
    // 4. 카메라 
    if (SpringArm) 
    {
       SpringArm->TargetOffset = FVector(0.0f, 0.0f, 150.0f); 
       SpringArm->ProbeSize = 12.0f; 
    }
    
    // 5. 방패 컴포넌트 생성
    ShieldMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShieldMesh"));
    ShieldMeshComp->SetupAttachment(GetMesh(), TEXT("Weapon_HitCenter"));
    
    //6. 방패 체력바 위젯 생성
    ShieldBarWidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("ShieldBarWidget"));
    ShieldBarWidgetComp->SetupAttachment(ShieldMeshComp); // 방패에 붙임
    ShieldBarWidgetComp->SetWidgetSpace(EWidgetSpace::World); // 월드 공간에 배치 (3D처럼)
    ShieldBarWidgetComp->SetDrawSize(FVector2D(100.0f, 15.0f)); // 크기 설정
    ShieldBarWidgetComp->SetVisibility(false);
    
    // 기본적으로 꺼둠 & 충돌 없음
    ShieldMeshComp->SetVisibility(false);
    ShieldMeshComp->SetCollisionProfileName(TEXT("NoCollision"));
    
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        ShieldMeshComp->SetStaticMesh(CubeMesh.Object);
        ShieldMeshComp->SetRelativeScale3D(FVector(0.1f, 2.5f, 3.5f)); 
        ShieldMeshComp->SetRelativeLocation(FVector(50.0f, 0.0f, 0.0f));
    }
}

void APaladinCharacter::BeginPlay()
{
    Super::BeginPlay();
    
    // 서버에서만 HP 초기화
    if (HasAuthority())
    {
        CurrentShieldHP = MaxShieldHP;
        bIsShieldBroken = false;
    }
    
    if (GetCapsuleComponent() && ShieldMeshComp)
    {
        GetCapsuleComponent()->IgnoreComponentWhenMoving(ShieldMeshComp, true);
    }
    
    // [최적화] 시작할 때 무기 컴포넌트 미리 찾아서 저장 (캐싱)
    TArray<UStaticMeshComponent*> StaticComps;
    GetComponents<UStaticMeshComponent>(StaticComps);
    for (UStaticMeshComponent* Comp : StaticComps)
    {
        // 소켓 이름이 맞는 컴포넌트를 찾으면 저장하고 루프 종료
        if (Comp && Comp->DoesSocketExist(TEXT("Weapon_HitCenter"))) 
        { 
            CachedWeaponMesh = Comp; 
            break; 
        }
    }
}

void APaladinCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    
    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (JobAbilityAction)
        {
            EIC->BindAction(JobAbilityAction, ETriggerEvent::Completed, this, &APaladinCharacter::EndJobAbility);
        }
    }
}

void APaladinCharacter::OnDeath()
{
    SetShieldActive(false);
    bIsGuarding = false;
    
    StopAnimMontage();
    
    Super::OnDeath();
}

void APaladinCharacter::Attack()
{
    if (!CanAttack()) return;
    
    // 방어중에는 공격 불가
    if (bIsGuarding) return;
    
    StartAttackCooldown();
    
    FName SkillName;
    if (CurrentComboIndex == 0) SkillName = TEXT("Attack_A");
    else SkillName = TEXT("Attack_B");
    
    ProcessSkill(SkillName);
    
    CurrentComboIndex++;
    if (CurrentComboIndex > 1) CurrentComboIndex = 0;
    
    GetWorldTimerManager().ClearTimer(ComboResetTimer);
    GetWorldTimerManager().SetTimer(ComboResetTimer, this, &APaladinCharacter::ResetCombo, 1.5f, false);
}

void APaladinCharacter::ResetCombo()
{
    CurrentComboIndex = 0;
}

// =========================================================
// 데이터 테이블 처리 & 판정 타이머 시작
// =========================================================
void APaladinCharacter::ProcessSkill(FName SkillRowName)
{
    if (!SkillDataTable) return;
    static const FString ContextString(TEXT("PaladinSkillContext"));
    FSkillData* Data = SkillDataTable->FindRow<FSkillData>(SkillRowName, ContextString);
    
    if (Data)
    {
        if (!IsSkillUnlocked(Data->RequiredStage)) return;
       
        CurrentSkillDamage = Data->Damage;
       
        // 1. 몽타주 재생 (화면상 즉각적인 반응을 위해)
        // 클라이언트: 내 화면에서 바로 칼 휘두름 (반응성 UP)
        // 서버: RPC 받고 들어와서 서버 화면에서 칼 휘두름
        if (Data->SkillMontage)
        {
            PlayAnimMontage(Data->SkillMontage);
        }

        // 2. [서버] 실제 판정 로직 및 다른 사람들에게 애니메이션 전파
        if (HasAuthority())
        {
            // A. 다른 클라이언트들에게도 "쟤 칼 휘둘렀어"라고 알려줌 (Multicast)
            if (Data->SkillMontage)
            {
                // 기존에 만드셨던 함수 그대로 사용
                Server_PlayMontage(Data->SkillMontage); 
            }

            // B. 공격 판정 타이머 시작 (서버에서만 돌아가야 함)
            GetWorldTimerManager().ClearTimer(AttackHitTimerHandle);
            GetWorldTimerManager().ClearTimer(HitLoopTimerHandle);

            float HitDelay = Data->ActionDelay; 

            if (HitDelay > 0.0f)
            {
                GetWorldTimerManager().SetTimer(
                   AttackHitTimerHandle,
                   this,
                   &APaladinCharacter::StartMeleeTrace, 
                   HitDelay,
                   false
                );
            }
            else
            {
                StartMeleeTrace();
            }
        }
        // 3. [클라이언트] 서버에게 "나 공격했으니 판정 로직 돌려줘" 요청
        else 
        {
            Server_ProcessSkill(SkillRowName);
        }
    }
}

void APaladinCharacter::Server_ProcessSkill_Implementation(FName SkillRowName)
{
    // 서버에서 다시 ProcessSkill을 호출 -> 
    // 이번엔 HasAuthority()가 True이므로 위쪽 2번(판정 로직)이 실행됨
    ProcessSkill(SkillRowName);
}

// =========================================================
// [1단계] 공격 판정 시작 (궤적 초기화)
// =========================================================
void APaladinCharacter::StartMeleeTrace()
{
    SwingDamagedActors.Empty();

    USceneComponent* WeaponComp = GetMesh();
    TArray<UStaticMeshComponent*> StaticComps;
    GetComponents<UStaticMeshComponent>(StaticComps);
    for (UStaticMeshComponent* Comp : StaticComps)
    {
        if (Comp && Comp->DoesSocketExist(TEXT("Weapon_HitCenter"))) { WeaponComp = Comp; break; }
    }

    if (WeaponComp)
    {
        LastHammerLocation = WeaponComp->GetSocketLocation(TEXT("Weapon_HitCenter"));
    }
    else
    {
        LastHammerLocation = GetActorLocation() + GetActorForwardVector() * 100.f;
    }

    GetWorldTimerManager().SetTimer(
        HitLoopTimerHandle,
        this,
        &APaladinCharacter::PerformMeleeTrace,
        0.015f, 
        true 
    );

    FTimerHandle StopTimer;
    GetWorldTimerManager().SetTimer(StopTimer, this, &APaladinCharacter::StopMeleeTrace, HitDuration, false);
}

// =========================================================
// [2단계] 반복 궤적 검사 (핵심 로직)
// =========================================================
void APaladinCharacter::PerformMeleeTrace()
{
    USceneComponent* WeaponComp = GetMesh();
    TArray<UStaticMeshComponent*> StaticComps;
    GetComponents<UStaticMeshComponent>(StaticComps);
    
    if (CachedWeaponMesh) 
    {
        WeaponComp = CachedWeaponMesh;
    }
    
    for (UStaticMeshComponent* Comp : StaticComps)
    {
        if (Comp && Comp->DoesSocketExist(TEXT("Weapon_HitCenter"))) { WeaponComp = Comp; break; }
    }

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

    TArray<FHitResult> HitResults;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    bool bHit = GetWorld()->SweepMultiByChannel(
        HitResults,
        LastHammerLocation, 
        CurrentLoc,         
        CurrentRot,         
        ECC_Pawn,
        FCollisionShape::MakeBox(HammerHitBoxSize),
        Params
    );

    if (bHit)
    {
        for (const FHitResult& Hit : HitResults)
        {
            AActor* HitActor = Hit.GetActor();
            
            if (HitActor && HitActor != this && !SwingDamagedActors.Contains(HitActor))
            {
                UGameplayStatics::ApplyDamage(
                    HitActor,
                    CurrentSkillDamage,
                    GetController(), 
                    this,            
                    UDamageType::StaticClass()
                );
                
                SwingDamagedActors.Add(HitActor);
            }
        }
    }

    LastHammerLocation = CurrentLoc;
}

// =========================================================
// [3단계] 판정 종료
// =========================================================
void APaladinCharacter::StopMeleeTrace()
{
    GetWorldTimerManager().ClearTimer(HitLoopTimerHandle);
    SwingDamagedActors.Empty();
}

// =========================================================
//   JobAbility (딜레이 적용 + 중복 제거됨)
// =========================================================
void APaladinCharacter::JobAbility()
{
    // 방패가 깨져있거나 HP가 없으면 실행 안 함
    if (bIsShieldBroken || CurrentShieldHP <= 0.0f) return;
    
    UAnimMontage* MontageToPlay = nullptr;
    float ActivationDelay = 0.0f;

    if (SkillDataTable)
    {
        static const FString ContextString(TEXT("PaladinGuardContext"));
        FSkillData* Data = SkillDataTable->FindRow<FSkillData>(FName("JobAbility"), ContextString);
        if (Data)
        {
            MontageToPlay = Data->SkillMontage;
            ActivationDelay = Data->ActionDelay; // 딜레이 시간 가져오기
        }
    }
    
    // 1. 애니메이션은 즉시 재생
    if (MontageToPlay)
    {
        PlayAnimMontage(MontageToPlay);
        Server_PlayMontage(MontageToPlay);
    }
    
    // 2. 방패 켜기는 딜레이 후 실행
    if (ActivationDelay > 0.0f)
    {
        GetWorldTimerManager().SetTimer(ShieldActivationTimer, this, &APaladinCharacter::ActivateGuard, ActivationDelay, false);
    }
    else
    {
        ActivateGuard();
    }
}

void APaladinCharacter::RestoreShieldAfterCooldown()
{
    // 타이머 정리
    GetWorldTimerManager().ClearTimer(ShieldActivationTimer);
    // 즉시 풀피 & 수리 완료
    CurrentShieldHP = MaxShieldHP;
    bIsShieldBroken = false;
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Blue,
        TEXT("Shield Fully Restored!"));
}

// 타이머가 호출하는 실제 방패 켜기 함수
void APaladinCharacter::ActivateGuard()
{
    Server_SetGuard(true);
}

// 3. 버튼 뗐을 때 (타이머 취소 포함)
void APaladinCharacter::EndJobAbility()
{
    // 딜레이 중에 버튼 떼면 타이머 취소 (방패 안 나오게)
    GetWorldTimerManager().ClearTimer(ShieldActivationTimer);

    Server_SetGuard(false);
}

void APaladinCharacter::Server_SetGuard_Implementation(bool bNewGuarding)
{
    if (bIsShieldBroken && bNewGuarding)
    {
        return;
    }
    
    bIsGuarding = bNewGuarding;
    
    // [서버 로직] 타이머 정리
    if (bIsGuarding)
    {
        // 방패 들었음 -> 회복 중단
        GetWorldTimerManager().ClearTimer(ShieldRegenTimer);
    }
    else
    {
        Multicast_StopMontage(0.25f);
        
        // 방패 내렸을 때 회복 로직
        // 1. 방패가 깨져서 쿨타임 대기중인가?
        bool bIsWaitingForRepair = GetWorldTimerManager().IsTimerActive(ShieldRegenTimer);
        // 2. 쿨타임 중이 아닐 때만 도트 힐 시작 
        if (!bIsWaitingForRepair && !GetWorldTimerManager().IsTimerActive(ShieldRegenTimer))
        {
            // 깨진게 아니라면, 내렸을 때 서서히 회복
            GetWorldTimerManager().SetTimer(ShieldRegenTimer, this, 
                &APaladinCharacter::RegenShield, 0.1f, true, ShieldRegenDelay);
        }
    }
    OnRep_IsGuarding();
}

void APaladinCharacter::OnRep_IsGuarding()
{
    SetShieldActive(bIsGuarding);
}

// 기본 충돌 로직
void APaladinCharacter::SetShieldActive(bool bActive)
{
    if (ShieldMeshComp)
    {
        ShieldMeshComp->SetVisibility(bActive);
        
        // 체력바 UI는 오직 나에게 (팔라딘 유저에게만) 보이게 함
        if (ShieldBarWidgetComp)
        {
            if (bActive && IsLocallyControlled())
            {
                ShieldBarWidgetComp->SetVisibility(true);
            }
            else
            {
                // 방패를 껐거나 남이 보는 내 캐릭터라면 UI 숨김
                ShieldBarWidgetComp->SetVisibility(false);
            }
        }
        if (bActive)
        {
            ShieldMeshComp->SetCollisionProfileName(TEXT("BlockAllDynamic"));
            
         
            // 카메라는 무시
            ShieldMeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
        }
        else
        {
            ShieldMeshComp->SetCollisionProfileName(TEXT("NoCollision"));
        }
    }
}

// 5. 데미지 로직 (서버 권한)
float APaladinCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    float ActualDamage = DamageAmount;

    // 서버이고, 방어 중이고, 방패가 멀쩡하고, 때린 놈이 있을 때
    if (HasAuthority() && bIsGuarding && !bIsShieldBroken && DamageCauser)
    {
        // 전방(내적 > 0) 체크
        FVector MyForward = GetActorForwardVector();
        FVector ToAttacker = (DamageCauser->GetActorLocation() - GetActorLocation()).GetSafeNormal();

        if (FVector::DotProduct(MyForward, ToAttacker) > -0.5f)
        {
            // 플레이어 데미지 무효화
            ActualDamage = 0.0f; 

            // 방패 HP 감소
            CurrentShieldHP -= DamageAmount;
            
            // 파괴 체크
            if (CurrentShieldHP <= 0.0f)
            {
                OnShieldBroken();
            }
        }
    }

    return Super::TakeDamage(ActualDamage, DamageEvent, EventInstigator, DamageCauser);
}

// 6. 방패 파괴 처리 (서버 전용)
void APaladinCharacter::OnShieldBroken()
{
    bIsShieldBroken = true;
    CurrentShieldHP = 0.0f;
    
    // 강제로 가드 해제 (Server_SetGuard 호출 -> OnRep 실행 -> 방패 꺼짐)
    Server_SetGuard(false);
    
    // (옵션) 파괴 사운드는 여기서 Multicast RPC로 뿌리면 됨
    UE_LOG(LogTemp, Warning, TEXT("Shield Broken!!"));
    
    // 돌아가던 도트 힐(회복) 타이머가 있다면 끔
    GetWorldTimerManager().ClearTimer(ShieldRegenTimer);
    
    // 데이터 테이블에서 쿨타임 가져오기
    float BrokenCooldown = 5.0f; // 팔라딘 직업능력 쿨타임 기본값
    
    if (SkillDataTable)
    {
        static const FString ContextString(TEXT("PaladinShieldBreak"));
        FSkillData* Data = SkillDataTable->FindRow<FSkillData>(FName("JobAbility"), ContextString);
        if (Data && Data->Cooldown > 0.0f)
        {
            BrokenCooldown = Data->Cooldown;
        }
    }
    
    // 5초 뒤에 RestoreShieldAfterCooldown 함수 실행하라고 예약
    GetWorldTimerManager().SetTimer(ShieldBrokenTimerHandle, this,
        &APaladinCharacter::RestoreShieldAfterCooldown, BrokenCooldown, false);
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
        TEXT("Shield is Broken! Waiting for cooldown..."));
}

// 7. 방패 회복 (서버 전용)
void APaladinCharacter::RegenShield()
{
    // 방어 중이면 회복 X
    if (bIsGuarding) return;

    CurrentShieldHP += ShieldRegenRate * 0.1f; // 0.1초마다

    // 최대치 도달 시
    if (CurrentShieldHP >= MaxShieldHP)
    {
        CurrentShieldHP = MaxShieldHP;
        bIsShieldBroken = false;
        GetWorldTimerManager().ClearTimer(ShieldRegenTimer); // 회복 끝
    }
    // 깨진 상태였는데 30% 이상 차면 '사용 가능' 상태로 복구
    else if (bIsShieldBroken && CurrentShieldHP > MaxShieldHP * 0.3f)
    {
        bIsShieldBroken = false;
    }
}

// 8. 리플리케이션 변수 등록
void APaladinCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    DOREPLIFETIME(APaladinCharacter, bIsGuarding);
    DOREPLIFETIME(APaladinCharacter, CurrentShieldHP);
    DOREPLIFETIME(APaladinCharacter, bIsShieldBroken);
}

// =========================================================
// 네트워크 RPC
// =========================================================
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
    // 현재 재생 중인 몽타주가 있다면 부드럽게 정지
    UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
    if (AnimInstance && AnimInstance->GetCurrentActiveMontage())
    {
        AnimInstance->Montage_Stop(BlendOutTime);
    }
}