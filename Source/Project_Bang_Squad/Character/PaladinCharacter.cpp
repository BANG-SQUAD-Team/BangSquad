#include "Project_Bang_Squad/Character/PaladinCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/DataTable.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DamageEvents.h"
#include "DrawDebugHelpers.h" // 디버그 그리기 필수

APaladinCharacter::APaladinCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    
    bReplicates = true;
    SetReplicateMovement(true);
    
    // 1. 팔라딘 무빙
    GetCharacterMovement()->MaxWalkSpeed = 450.f;
    GetCharacterMovement()->JumpZVelocity = 550.f;
    GetCharacterMovement()->AirControl = 0.35f;
    
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
    
    // 기본적으로 꺼둠 & 충돌 없음
    ShieldMeshComp -> SetVisibility(false);
    ShieldMeshComp->SetCollisionProfileName(TEXT("NoCollision"));
    
    // 큐브 모양 임시 설정 (나중에 블루프린트에서 납작한 판으로 교체)
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        ShieldMeshComp->SetStaticMesh(CubeMesh.Object);
        ShieldMeshComp->SetRelativeScale3D(FVector(0.1f,2.5f,3.5f)); // 납작하고 넓게
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
}

void APaladinCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    
    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        // JobAbilityAction은 protected 변수라 자식이 접근 가능
        if (JobAbilityAction)
        {
            EIC->BindAction(JobAbilityAction, ETriggerEvent::Completed, this, &APaladinCharacter::EndJobAbility);
        }
        
    }
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
       
       if (Data->SkillMontage)
       {
          PlayAnimMontage(Data->SkillMontage);
          Server_PlayMontage(Data->SkillMontage);
       }

       if (HasAuthority())
       {
          // 기존 타이머들 초기화
          GetWorldTimerManager().ClearTimer(AttackHitTimerHandle);
          GetWorldTimerManager().ClearTimer(HitLoopTimerHandle);

          // 데이터 테이블에서 딜레이 시간 가져오기
          // (주의: BaseCharacter.h에 변수명이 ProjectileSpawnDelay라면 그걸로 수정하세요)
          float HitDelay = Data->ActionDelay; 

          if (HitDelay > 0.0f)
          {
             GetWorldTimerManager().SetTimer(
                AttackHitTimerHandle,
                this,
                &APaladinCharacter::StartMeleeTrace, // ExecuteMeleeHit 대신 Start 호출
                HitDelay,
                false
             );
          }
          else
          {
             StartMeleeTrace();
          }
       }
    }
}


// =========================================================
// [1단계] 공격 판정 시작 (궤적 초기화)
// =========================================================
void APaladinCharacter::StartMeleeTrace()
{
    // 중복 타격 목록 초기화
    SwingDamagedActors.Empty();

    // 초기 위치 잡기
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

    // 루프 타이머 시작 (0.015초마다 검사 = 매우 부드러움)
    GetWorldTimerManager().SetTimer(
        HitLoopTimerHandle,
        this,
        &APaladinCharacter::PerformMeleeTrace,
        0.015f, 
        true 
    );

    // 일정 시간(HitDuration) 뒤에 판정 종료 예약
    FTimerHandle StopTimer;
    GetWorldTimerManager().SetTimer(StopTimer, this, &APaladinCharacter::StopMeleeTrace, HitDuration, false);
}

// =========================================================
// [2단계] 반복 궤적 검사 (핵심 로직)
// =========================================================
void APaladinCharacter::PerformMeleeTrace()
{
    // 무기 컴포넌트 및 소켓 위치 찾기
    USceneComponent* WeaponComp = GetMesh();
    TArray<UStaticMeshComponent*> StaticComps;
    GetComponents<UStaticMeshComponent>(StaticComps);
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

    // [Sweep] 이전 위치(Last) -> 현재 위치(Current) 연결해서 검사
    TArray<FHitResult> HitResults;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    bool bHit = GetWorld()->SweepMultiByChannel(
        HitResults,
        LastHammerLocation, // 시작점
        CurrentLoc,         // 끝점 (이 사이를 메꿔서 검사함)
        CurrentRot,         // 회전
        ECC_Pawn,
        FCollisionShape::MakeBox(HammerHitBoxSize),
        Params
    );

    // [디버그] 궤적 그리기 (주석 해제 시 보라색 잔상 보임)
     DrawDebugBox(GetWorld(), CurrentLoc, HammerHitBoxSize, CurrentRot, FColor::Purple, false, 0.5f);
     DrawDebugLine(GetWorld(), LastHammerLocation, CurrentLoc, FColor::Red, false, 0.5f, 0, 2.0f);

    if (bHit)
    {
        for (const FHitResult& Hit : HitResults)
        {
            AActor* HitActor = Hit.GetActor();
            
            //  이번 스윙에 이미 맞은 놈은 건너뛰기
            if (HitActor && HitActor != this && !SwingDamagedActors.Contains(HitActor))
            {
                UGameplayStatics::ApplyDamage(
                    HitActor,
                    CurrentSkillDamage,
                    GetController(), 
                    this,            
                    UDamageType::StaticClass()
                );
                
                // 목록에 추가
                SwingDamagedActors.Add(HitActor);
            }
        }
    }

    // 다음 프레임을 위해 현재 위치를 '과거'로 저장
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

void APaladinCharacter::JobAbility()
{
    // 방패가 깨져있거나 HP가 바닥이면 요청도 안 보냄
    if (bIsShieldBroken || CurrentShieldHP <= 0.0f) return;
    
    // 데이터 테이블 읽기
    if (SkillDataTable)
    {
        static const FString ContextString(TEXT("PaladinGuardContext"));
        FSkillData* Data = SkillDataTable->FindRow<FSkillData>(FName("Paladin_Guard"), ContextString);
        if (Data)
        {
            GuardWalkSpeed = Data -> Duration > 0.0f ? Data->Duration : 250.0f;
        }
    }
    
    Server_SetGuard(true);
}

// 방어 해제 (클라이언트 입력)
void APaladinCharacter::EndJobAbility()
{
    Server_SetGuard(false);
}

void APaladinCharacter::Server_SetGuard_Implementation(bool bNewGuarding)
{
    // 방패가 깨진 상태면 강제로 false 처리
    if (bIsShieldBroken && bNewGuarding)
    {
        return;
    }
    
    bIsGuarding = bNewGuarding;
    
    // [서버 로직] 타이머 정리
    if (bIsGuarding)
    {
        // 방패 들었음 -> 회복 중단
        GetWorld()->GetTimerManager().ClearTimer(ShieldRegenTimer);
    }
    else
    {
        // 방패 내렸음 ->  Delay 뒤에 회복 시작
        // 타이머 중복 실행 방지
        if (!GetWorld()->GetTimerManager().IsTimerActive(ShieldRegenTimer))
        {
            GetWorld()->GetTimerManager().SetTimer(ShieldRegenTimer, this , &APaladinCharacter::RegenShield,
                0.1f, true, ShieldRegenDelay);
        }
    }
    OnRep_IsGuarding();
}

void APaladinCharacter::OnRep_IsGuarding()
{
    // 이동속도 
    if (bIsGuarding) GetCharacterMovement()->MaxWalkSpeed = GuardWalkSpeed;
    else GetCharacterMovement()->MaxWalkSpeed = GuardWalkSpeed = 450.0f;
    
    // 방패 메쉬 켜기/ 끄기
    SetShieldActive(bIsGuarding) ;
}

// [보조] 방패 활성/비활성 처리
void APaladinCharacter::SetShieldActive(bool bActive)
{
    if (ShieldMeshComp)
    {
        ShieldMeshComp->SetVisibility(bActive);
        // 켜지면 물리 충돌(BlockAllDynamic), 꺼지면 통과(NoCollision)
        ShieldMeshComp->SetCollisionProfileName(bActive ? TEXT("BlockAllDynamic") : TEXT("NoCollision"));
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

        if (FVector::DotProduct(MyForward, ToAttacker) > 0.0f)
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