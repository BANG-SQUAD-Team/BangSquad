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
}

void APaladinCharacter::BeginPlay()
{
    Super::BeginPlay();
}

void APaladinCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    
    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
       if (JumpAction)
       {
          EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
          EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
       }
       if (AttackAction)
       {
          EIC->BindAction(AttackAction, ETriggerEvent::Started, this, &APaladinCharacter::Attack);
       }
    }
}

void APaladinCharacter::Attack()
{
    if (!CanAttack()) return;
    
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