#include "Project_Bang_Squad/Character/TitanCharacter.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h" 
#include "TimerManager.h"
#include "Engine/DataTable.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/SpringArmComponent.h"
#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h"
#include "Project_Bang_Squad/Character/Enemy/EnemyNormal.h"
#include "Project_Bang_Squad/Character/Player/Titan/TitanRock.h"
#include "Project_Bang_Squad/Character/Player/Titan/TitanThrowableActor.h"
#include "Project_Bang_Squad/Character/MageCharacter.h"
#include "Engine/StaticMeshActor.h"
#include "Particles/ParticleSystemComponent.h"
#include "NiagaraFunctionLibrary.h" 
#include "NiagaraComponent.h"

ATitanCharacter::ATitanCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

        // 이동 속도 설정
        if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
        {
            MoveComp->MaxWalkSpeed = 495.f;
        }
    
    // 투사체 생성 위치 컴포넌트
    ThrowSpawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("ThrowSpawnPoint"));

    if (Camera)
    {
       ThrowSpawnPoint->SetupAttachment(Camera);
    }
    else
    {
       ThrowSpawnPoint->SetupAttachment(GetRootComponent());
    }

    ThrowSpawnPoint->ArrowSize = 0.5f;
    ThrowSpawnPoint->ArrowColor = FColor::Cyan;

    // 근접 공격 판정 박스 크기 초기화
    HitBoxSize = FVector(80.0f, 80.0f, 80.0f);
    
    // 궤적 스플라인 컴포넌트 생성 및 초기화
    TrajectorySpline = CreateDefaultSubobject<USplineComponent>(TEXT("TrajectorySpline"));
    TrajectorySpline->SetupAttachment(RootComponent);
    TrajectorySpline->SetVisibility(false);

    DashPSC = CreateDefaultSubobject<UNiagaraComponent>(TEXT("DashFX"));

    if (GetMesh())
    {
        DashPSC->SetupAttachment(GetMesh(), TEXT("spine_03"));
    }
    else
    {
        DashPSC->SetupAttachment(RootComponent);
    }

    DashPSC->bAutoActivate = false;

    CharacterSpecificAccessoryOffset = FVector(0.0f, 30.0f, 0.0f);
}

void ATitanCharacter::BeginPlay()
{
    Super::BeginPlay();

    GetCapsuleComponent()->OnComponentBeginOverlap.AddDynamic(this, &ATitanCharacter::OnChargeOverlap);
    GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &ATitanCharacter::OnChargeHit);

    if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
    {
        AnimInst->OnMontageEnded.AddDynamic(this, &ATitanCharacter::OnMontageEndedDelegate);
    }
    
    if (SpringArm) 
    {
       DefaultSocketOffset = SpringArm->SocketOffset;
    }
}

void ATitanCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 로컬 플레이어 전용 로직
    if (IsLocallyControlled())
    {
        FString GrabActorName = GrabbedActor ? *GrabbedActor->GetName() : TEXT("None");

        // 어떤 플래그가 True인지 색상으로 구분하여 표시
        FString DebugMsg = FString::Printf(TEXT("[TITAN DEBUG]\nAttacking: %s\nGrabbing: %s\nCooldown: %s\nGrabbedTarget: %s"),
            bIsAttacking ? TEXT("TRUE (RED)") : TEXT("False"),
            bIsGrabbing ? TEXT("TRUE (RED)") : TEXT("False"),
            bIsCooldown ? TEXT("TRUE (YELLOW)") : TEXT("False"),
            *GrabActorName);

        // 화면에 출력 (키: 1, 지속시간: 0.0f(매 프레임 갱신), 색상: Cyan)
        GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Cyan, DebugMsg);

        // 1. 카메라 보간
        if (SpringArm)
        {
            FVector TargetOffset = bIsGrabbing ? AimingSocketOffset : DefaultSocketOffset;
            FVector CurrentOffset = SpringArm->SocketOffset;
            FVector NewOffset = FMath::VInterpTo(CurrentOffset, TargetOffset, DeltaTime, CameraInterpSpeed);
            SpringArm->SocketOffset = NewOffset;
        }

        // =================================================================
        // [상태 강제 초기화 (Self-Healing)] 
        // =================================================================

        // A. [잡기 복구] 잡은 놈이 없는데 잡고 있다고 착각할 때
        bool bHasValidTarget = (GrabbedActor != nullptr && IsValid(GrabbedActor));
        if (bIsGrabbing && !bHasValidTarget)
        {
            bIsGrabbing = false;
            GrabbedActor = nullptr;
            ShowTrajectory(false);
            if (HoveredActor)
            {
                SetHighlight(HoveredActor, false);
                HoveredActor = nullptr;
            }

            StopAnimMontage();
        }

        // B. [공격 복구] 공격 중이라는데 몸이 가만히 있을 때 (몽타주 끊김)
        if (bIsAttacking)
        {
            UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
            // 몽타주가 아무것도 재생 안 되고 있다면 -> 즉시 공격 상태 해제
            if (AnimInstance && !AnimInstance->IsAnyMontagePlaying())
            {
                bIsAttacking = false;
            }
        }

        // C. [쿨타임 복구] 쿨타임이라는데 타이머가 안 돌 때
        if (bIsAttackCoolingDown)
        {
            // 타이머가 돌고 있지 않다면 -> 쿨타임 해제
            if (!GetWorldTimerManager().IsTimerActive(AttackCooldownTimerHandle))
            {
                bIsAttackCoolingDown = false;
            }
        }
        // =================================================================

        // 3. 궤적 및 하이라이트 표시
        if (!bIsGrabbing)
        {
            UpdateHoverHighlight(); // 상태가 풀렸으니 이제 이게 실행됩니다!
            ShowTrajectory(false);
        }
        else
        {
            ShowTrajectory(true);
            UpdateTrajectory();
        }
    }
}

void ATitanCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ATitanCharacter, GrabbedActor);
}

void ATitanCharacter::OnDeath()
{
    if (bIsDead) return;

    // 사망 시 잡고 있던 물체 자동 던지기
    if (HasAuthority() && bIsGrabbing && GrabbedActor)
    {
       AutoThrowTimeout();
    }

    if (bIsCharging)
    {
       StopCharge();
    }

    if (HoveredActor)
    {
       SetHighlight(HoveredActor, false);
       HoveredActor = nullptr;
    }

    // 모든 타이머 정리
    GetWorldTimerManager().ClearTimer(GrabTimerHandle);
    GetWorldTimerManager().ClearTimer(ChargeTimerHandle);
    GetWorldTimerManager().ClearTimer(CooldownTimerHandle);
    GetWorldTimerManager().ClearTimer(Skill2CooldownTimerHandle);
    GetWorldTimerManager().ClearTimer(Skill1CooldownTimerHandle);
    GetWorldTimerManager().ClearTimer(RockThrowTimerHandle);
    GetWorldTimerManager().ClearTimer(AttackHitTimerHandle);
    GetWorldTimerManager().ClearTimer(HitLoopTimerHandle);
    GetWorldTimerManager().ClearTimer(MeleeStopTimerHandle);

    // 상태 플래그 초기화
    bIsGrabbing = false;
    GrabbedActor = nullptr;
    ShowTrajectory(false); 

    bIsCooldown = false;          
    bIsSkill1Cooldown = false;    
    bIsSkill2Cooldown = false;    
    bIsCharging = false;          

    // 데이터 초기화
    GetWorldTimerManager().ClearAllTimersForObject(this);
    SwingDamagedActors.Empty();
    HitVictims.Empty();

    StopAnimMontage();
    Super::OnDeath();
}

// =================================================================
// [입력 및 공격 처리]
// =================================================================

void ATitanCharacter::Attack()
{
    if (bIsDead) return;

    if (!CanAttack())
    {
        UE_LOG(LogTemp, Warning, TEXT("!!! Attack Blocked !!! -> Attacking:%d, Grabbing:%d, Cooldown:%d"),
            bIsAttacking, bIsGrabbing, bIsCooldown);
        return;
    }

    if (IsLocallyControlled())
    {
        UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
        if (bIsAttacking && AnimInstance && !AnimInstance->IsAnyMontagePlaying())
        {
            bIsAttacking = false;
        }
    }

    if (!CanAttack()) return;

    if (bIsGrabbing && !IsValid(GrabbedActor))
    {
        bIsGrabbing = false;
        GrabbedActor = nullptr;
        ShowTrajectory(false);

        StopAnimMontage();
    }

    FName SkillRowName = bIsNextAttackA ? TEXT("Attack_A") : TEXT("Attack_B");

    if (SkillDataTable)
    {
       static const FString ContextString(TEXT("TitanAttack_Local"));
       FSkillData* Row = SkillDataTable->FindRow<FSkillData>(SkillRowName, ContextString);
       if (Row && Row->Cooldown > 0.0f)
       {
          AttackCooldownTime = Row->Cooldown;
       }
    }
    StartAttackCooldown(); 

    Server_Attack(SkillRowName);
    bIsNextAttackA = !bIsNextAttackA;
}

void ATitanCharacter::Server_Attack_Implementation(FName SkillName)
{
    ForceRecoverState();
    
    if (SkillName == TEXT("Attack_A"))
    {
       MyAttackSocket = TEXT("Hand_R_Socket"); 
    }
    else
    {
       MyAttackSocket = TEXT("Hand_L_Socket"); 
    }

    float ActionDelay = 0.0f;

    if (SkillDataTable)
    {
       static const FString ContextString(TEXT("TitanAttack"));
       FSkillData* Row = SkillDataTable->FindRow<FSkillData>(SkillName, ContextString);

       if (Row)
       {
          CurrentSkillDamage = Row->GetRandomizedDamage();
          if (Row->Cooldown > 0.0f) AttackCooldownTime = Row->Cooldown;
          ActionDelay = Row->ActionDelay;
       }
    }

    StartAttackCooldown();
    Multicast_Attack(SkillName);

    // 기존 타이머 정리
    GetWorldTimerManager().ClearTimer(AttackHitTimerHandle);
    GetWorldTimerManager().ClearTimer(HitLoopTimerHandle);

    // ★ 추가된 부분: 이전 공격의 종료 예약 타이머가 살아있다면 죽이기 ★
    GetWorldTimerManager().ClearTimer(MeleeStopTimerHandle);

    if (ActionDelay > 0.0f)
    {
        GetWorldTimerManager().SetTimer(AttackHitTimerHandle, this, &ATitanCharacter::StartMeleeTrace, ActionDelay, false);
    }
    else
    {
        StartMeleeTrace();
    }
}

void ATitanCharacter::Multicast_Attack_Implementation(FName SkillName)
{
    bIsAttacking = true;
    ProcessSkill(SkillName);
}

// =========================================================
// [근접 공격 판정 로직]
// =========================================================

void ATitanCharacter::StartMeleeTrace()
{
    SwingDamagedActors.Empty();

    if (GetMesh() && GetMesh()->DoesSocketExist(MyAttackSocket))
    {
        LastHandLocation = GetMesh()->GetSocketLocation(MyAttackSocket);
    }
    else
    {
        LastHandLocation = GetActorLocation() + GetActorForwardVector() * 100.f;
    }

    // 판정 루프 시작
    GetWorldTimerManager().SetTimer(HitLoopTimerHandle, this, &ATitanCharacter::PerformMeleeTrace, 0.015f, true);

    // ★ 수정된 부분: 지역 변수 삭제하고 멤버 변수 사용 ★
    GetWorldTimerManager().SetTimer(MeleeStopTimerHandle, this, &ATitanCharacter::StopMeleeTrace, HitDuration, false);
}

void ATitanCharacter::PerformMeleeTrace()
{
    FVector CurrentLoc;
    FQuat CurrentRot;

    if (GetMesh() && GetMesh()->DoesSocketExist(MyAttackSocket))
    {
       CurrentLoc = GetMesh()->GetSocketLocation(MyAttackSocket);
       CurrentRot = GetMesh()->GetSocketQuaternion(MyAttackSocket);
    }
    else
    {
       CurrentLoc = GetActorLocation() + GetActorForwardVector() * 200.f;
       CurrentRot = GetActorQuat();
    }

    TArray<FHitResult> HitResults;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    bool bHit = GetWorld()->SweepMultiByChannel(
       HitResults,
       LastHandLocation,
       CurrentLoc,
       CurrentRot,
       ECC_Pawn,
       FCollisionShape::MakeBox(HitBoxSize),
       Params
    );

    if (bHit)
    {
       for (const FHitResult& Hit : HitResults)
       {
          AActor* HitActor = Hit.GetActor();

          if (HitActor && HitActor != this && !SwingDamagedActors.Contains(HitActor))
          {
             if (HitActor->ActorHasTag("Player")) continue;

             float Dmg = (CurrentSkillDamage > 0.0f) ? CurrentSkillDamage : 10.0f;
             UGameplayStatics::ApplyDamage(
                HitActor,
                Dmg,
                GetController(),
                this,
                UDamageType::StaticClass()
             );

             SwingDamagedActors.Add(HitActor);
          }
       }
    }

    LastHandLocation = CurrentLoc;
}

void ATitanCharacter::StopMeleeTrace()
{
    GetWorldTimerManager().ClearTimer(HitLoopTimerHandle);
    // ★ 추가: 자기 자신(종료 타이머)도 명시적으로 클리어 (이미 만료되었겠지만 안전하게)
    GetWorldTimerManager().ClearTimer(MeleeStopTimerHandle);
    SwingDamagedActors.Empty();
}

void ATitanCharacter::Multicast_FixMesh_Implementation(ACharacter* Victim)
{
    if (!Victim || !Victim->IsValidLowLevel()) return;

    // 클라이언트 캐릭터 상태 강제 동기화 (충돌/이동 켜기)
    if (Victim->GetCapsuleComponent())
    {
       Victim->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }

    if (Victim->GetCharacterMovement())
    {
       Victim->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
       Victim->GetCharacterMovement()->GravityScale = 1.0f;
       Victim->GetCharacterMovement()->Velocity = FVector::ZeroVector; 
    }

    FRotator CurrentRot = Victim->GetActorRotation();
    Victim->SetActorRotation(FRotator(0.f, CurrentRot.Yaw, 0.f));

    if (Victim->GetMesh())
    {
       Victim->GetMesh()->SetRelativeLocationAndRotation(
          FVector(0.f, 0.f, -90.f),
          FRotator(0.f, -90.f, 0.f)
       );
    }
    Victim->ForceNetUpdate();
}

// =========================================================
// [잡기 / 던지기 (JobAbility)]
// =========================================================

void ATitanCharacter::JobAbility()
{
    if (!IsSkillUnlocked(1)) return;
    if (bIsDead || bIsCooldown) return;

    if (bIsGrabbing && !IsValid(GrabbedActor))
    {
        bIsGrabbing = false;
        GrabbedActor = nullptr;
    }

    if (bIsGrabbing)
    {
       // 던지기 실행
       if (bIsCooldown) return;
       bIsCooldown = true;
       
       float LocalThrowCooldown = 3.0f; 
       if (SkillDataTable)
       {
          FSkillData* Row = SkillDataTable->FindRow<FSkillData>(TEXT("JobAbility"), TEXT("TitanJob_Local"));
          if (Row && Row->Cooldown > 0.0f) LocalThrowCooldown = Row->Cooldown;
       }
       
        // 클라이언트 UI 쿨타임 즉시 실행 (3번 슬롯)
        TriggerSkillCooldown(3, LocalThrowCooldown);
        
       GetWorldTimerManager().SetTimer(CooldownTimerHandle, this, &ATitanCharacter::ResetCooldown, LocalThrowCooldown, false);
       FVector ThrowOrigin = ThrowSpawnPoint->GetComponentLocation();
       Server_ThrowTarget(ThrowOrigin);
    }
    else
    {
       // 잡기 시도
       if (!CanAttack()) return;
       if (bIsCooldown) return;
       if (HoveredActor) Server_TryGrab(HoveredActor);
    }
}

void ATitanCharacter::Skill1()
{
    if (!CanAttack()) return;
    if (bIsDead || bIsSkill1Cooldown) return;

    float LocalCooldown = 3.0f;
    if (SkillDataTable)
    {
       static const FString ContextString(TEXT("TitanSkill1_Local"));
       FSkillData* Row = SkillDataTable->FindRow<FSkillData>(TEXT("Skill1"), ContextString);
        if (Row)
        {
            // 해금되지 않은 스킬이면 여기서 중단! (UI도 안 뜨고 서버 요청도 안 함)
            if (!IsSkillUnlocked(Row->RequiredStage)) return;

            if (Row->Cooldown > 0.0f) LocalCooldown = Row->Cooldown;
        }
    }

    bIsSkill1Cooldown = true;
    TriggerSkillCooldown(1, LocalCooldown);
    GetWorldTimerManager().SetTimer(Skill1CooldownTimerHandle, this, &ATitanCharacter::ResetSkill1Cooldown, LocalCooldown, false);
    Server_Skill1();
}

void ATitanCharacter::Skill2()
{
    if (!CanAttack()) return;
    if (bIsDead || bIsSkill2Cooldown) return;

    float LocalCooldown = 5.0f; 
    if (SkillDataTable)
    {
       static const FString ContextString(TEXT("TitanSkill2_Local"));
       FSkillData* Row = SkillDataTable->FindRow<FSkillData>(TEXT("Skill2"), ContextString);
        if (Row)
        {
            // 해금 체크
            if (!IsSkillUnlocked(Row->RequiredStage)) return;

            if (Row->Cooldown > 0.0f) LocalCooldown = Row->Cooldown;
        }
    }

    bIsSkill2Cooldown = true;
    TriggerSkillCooldown(2, LocalCooldown);
    GetWorldTimerManager().SetTimer(Skill2CooldownTimerHandle, this, &ATitanCharacter::ResetSkill2Cooldown, LocalCooldown, false);
    Server_Skill2();
}

void ATitanCharacter::ExecuteGrab() { if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Magenta, TEXT("ExecuteGrab Called")); }

void ATitanCharacter::Server_TryGrab_Implementation(AActor* TargetToGrab)
{
    if (!TargetToGrab || bIsGrabbing) return;

    ForceRecoverState();

    // 1. 유효성 검사 (기존 코드)
    if (!TargetToGrab || bIsGrabbing) return;

    if (TargetToGrab->IsA(AEnemyCharacterBase::StaticClass()) && !TargetToGrab->IsA(AEnemyNormal::StaticClass()))
    {
        return;
    }

    float DistSq = FVector::DistSquared(GetActorLocation(), TargetToGrab->GetActorLocation());
    if (DistSq > 1000.f * 1000.f) return;

    // 2. 상태 변경
    GrabbedActor = TargetToGrab;
    bIsGrabbing = true;

    // =================================================================
    // [핵심 수정] 서버에서 먼저 끄고, 모두에게 알림!
    // =================================================================

    // 1) 일단 서버에서 위치 갱신 끄기 (중요)
    GrabbedActor->SetReplicateMovement(false);

    // 2) 모든 클라이언트에게 "당장 물리 끄고 손에 붙여!" 명령
    Multicast_GrabSuccess(GrabbedActor);

    // 3) 캐릭터일 경우 상태 설정 (기존 코드)
    if (ACharacter* Victim = Cast<ACharacter>(GrabbedActor))
    {
        SetHeldState(Victim, true);
    }

    // 4) 몽타주 및 타이머 (기존 코드)
    Multicast_PlayJobMontage(TEXT("Grab"));
    GetWorldTimerManager().SetTimer(GrabTimerHandle, this, &ATitanCharacter::AutoThrowTimeout, GrabMaxDuration, false);
}

void ATitanCharacter::Multicast_GrabSuccess_Implementation(AActor* Target)
{
    if (!Target || !Target->IsValidLowLevel()) return;

    // 1. 확실하게 물리 끄기
    if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(Target->GetRootComponent()))
    {
        RootComp->SetSimulatePhysics(false);
        RootComp->SetCollisionProfileName(TEXT("NoCollision"));
        RootComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // 2. 위치 동기화 끄기 (클라이언트에서도 꺼야 덜덜 떨리지 않음)
    Target->SetReplicateMovement(false);

    // 3. 손에 강제 부착 (SnapToTarget)
    Target->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Hand_R_Socket"));

    // 4. 위치/회전 0점으로 초기화 (혹시 모르니 손 중앙으로 정렬)
    Target->SetActorRelativeLocation(FVector::ZeroVector);
    Target->SetActorRelativeRotation(FRotator::ZeroRotator);
}

void ATitanCharacter::AutoThrowTimeout()
{ 
    FVector Forward = GetActorForwardVector(); 
    FVector ThrowOrigin = GetActorLocation() + FVector(0.f, 0.f, 60.f) + (Forward * 200.0f);
    Server_ThrowTarget(ThrowOrigin); 
}

void ATitanCharacter::OnRep_GrabbedActor()
{
    if (GrabbedActor)
    {
       // [잡혔을 때] 손에 부착 및 물리 비활성화
       bIsGrabbing = true;

       if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(GrabbedActor->GetRootComponent()))
       {
          RootComp->SetSimulatePhysics(false);
          RootComp->SetCollisionProfileName(TEXT("NoCollision"));
       }

       GrabbedActor->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Hand_R_Socket"));

       if (ACharacter* Victim = Cast<ACharacter>(GrabbedActor))
       {
          SetHeldState(Victim, true);
       }
    }
    else
    {
        bIsGrabbing = false;

        TArray<AActor*> AttachedActors;
        GetAttachedActors(AttachedActors);

        for (AActor* Child : AttachedActors)
        {
            if (Child && Child->GetRootComponent() && Child->GetRootComponent()->GetAttachSocketName() == TEXT("Hand_R_Socket"))
            {
                Child->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

                // 1. 캐릭터인지 확인
                if (ACharacter* Victim = Cast<ACharacter>(Child))
                {
                    // [캐릭터 복구 로직]
                    if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(Child->GetRootComponent()))
                    {
                        RootComp->SetCollisionProfileName(TEXT("Pawn")); // 캐릭터는 Pawn
                        RootComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                        RootComp->SetSimulatePhysics(true);
                    }

                    SetHeldState(Victim, false);

                    if (Victim->GetMesh())
                    {
                        Victim->GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));
                    }
                    if (Victim->GetCharacterMovement())
                    {
                        Victim->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
                    }
                }
                else
                {
                    Child->SetReplicateMovement(true);

                    if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(Child->GetRootComponent()))
                    {
                        RootComp->SetCollisionProfileName(TEXT("PhysicsActor"));
                        RootComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                        RootComp->SetSimulatePhysics(true);
                    }
                }
            }
        }
    }
}

void ATitanCharacter::PerformRadialImpact(FVector Origin, float Radius, float Damage, float RadialForce, AActor* IgnoreTarget)
{
    TArray<AActor*> IgnoreActors;
    IgnoreActors.Add(this); 
    if (IgnoreTarget)
    {
       IgnoreActors.Add(IgnoreTarget);
    }

    TArray<AActor*> OverlappedActors;
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));

    UKismetSystemLibrary::SphereOverlapActors(
       GetWorld(),
       Origin,
       Radius,
       ObjectTypes,
       AActor::StaticClass(),
       IgnoreActors,
       OverlappedActors
    );

    for (AActor* Victim : OverlappedActors)
    {
        if (!Victim || !Victim->IsValidLowLevel()) continue;

        // 데미지 적용
        if (!Victim->ActorHasTag("Player"))
        {
            UGameplayStatics::ApplyDamage(Victim, Damage, GetController(), this, UDamageType::StaticClass());
        }

        // [핵심 로직] 보스 판별 (상속 구조 활용)
        bool bIsBoss = Victim->IsA(AEnemyCharacterBase::StaticClass()) && !Victim->IsA(AEnemyNormal::StaticClass());

        // 보스가 아닐 때만 넉백 적용
        if (!bIsBoss)
        {
            FVector LaunchDir = (Victim->GetActorLocation() - Origin).GetSafeNormal();
            LaunchDir.Z = 0.5f;
            LaunchDir.Normalize();

            if (ACharacter* VictimChar = Cast<ACharacter>(Victim))
            {
                VictimChar->LaunchCharacter(LaunchDir * RadialForce, true, true);
            }
            else if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(Victim->GetRootComponent()))
            {
                if (RootComp->IsSimulatingPhysics())
                {
                    RootComp->AddImpulse(LaunchDir * RadialForce * 100.0f);
                }
            }
        }
    }
}

void ATitanCharacter::OnThrownActorHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit)
{
    if (OtherActor == this) return;

    if (SelfActor)
    {
        SelfActor->OnActorHit.RemoveDynamic(this, &ATitanCharacter::OnThrownActorHit);
    }

    float ImpactRadius = 300.0f;
    float KnockbackForce = 800.0f;
    float ImpactDamage = (CurrentSkillDamage > 0.0f) ? CurrentSkillDamage : 30.0f;

    // =================================================================
    // [최종 수정] "적(Enemy)"일 때만 데미지 1.5배! (제일 깔끔함)
    // =================================================================
    if (SelfActor)
    {
        // 몬스터(EnemyCharacterBase)로 변환이 성공하면 -> 적이다!
        if (Cast<AEnemyCharacterBase>(SelfActor) != nullptr)
        {
            UGameplayStatics::ApplyDamage(
                SelfActor,
                ImpactDamage * 1.5f, // 몬스터는 아프게 맞음
                GetController(),
                this,
                UDamageType::StaticClass()
            );
        }
    }
    // =================================================================

    // 주변 폭발 (SelfActor 제외)
    PerformRadialImpact(Hit.ImpactPoint, ImpactRadius, ImpactDamage, KnockbackForce, SelfActor);

    // 던져진 캐릭터 기상 처리 (적, 아군 모두 일어나는 건 해야 함)
    if (ACharacter* ThrownChar = Cast<ACharacter>(SelfActor))
    {
        if (ThrownChar->GetCharacterMovement())
        {
            ThrownChar->GetCharacterMovement()->StopMovementImmediately();
        }
        RecoverCharacter(ThrownChar);
    }
}

void ATitanCharacter::Multicast_ForceThrowCleanup_Implementation(AActor* TargetActor, FVector Velocity)
{
    if (!TargetActor || !TargetActor->IsValidLowLevel()) return;

    // 강제 부착 해제
    TargetActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    // [CASE A] 캐릭터 처리
    if (ACharacter* Victim = Cast<ACharacter>(TargetActor))
    {
        if (Victim->GetMesh())
        {
            Victim->GetMesh()->SetRelativeLocationAndRotation(
                FVector(0.f, 0.f, -90.f),
                FRotator(0.f, -90.f, 0.f)
            );
        }

        if (Victim->GetCapsuleComponent())
        {
            Victim->GetCapsuleComponent()->SetSimulatePhysics(false);
            Victim->GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
            Victim->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        }

        if (Victim->GetCharacterMovement())
        {
            Victim->GetCharacterMovement()->StopMovementImmediately();
            Victim->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
            Victim->GetCharacterMovement()->GravityScale = 1.0f;
        }

        SetHeldState(Victim, false);

        if (!Victim->IsLocallyControlled())
        {
            Victim->LaunchCharacter(Velocity, true, true);
        }
    }
    // [CASE B] 물리 액터(버섯) 처리
    else if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(TargetActor->GetRootComponent()))
    {
        RootComp->SetSimulatePhysics(true);
        RootComp->SetCollisionProfileName(TEXT("PhysicsActor"));
        RootComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        RootComp->WakeAllRigidBodies();

        if (GetCapsuleComponent())
        {
            GetCapsuleComponent()->IgnoreComponentWhenMoving(RootComp, true);
            RootComp->IgnoreComponentWhenMoving(GetCapsuleComponent(), true);
        }

        RootComp->AddImpulse(Velocity, NAME_None, true);
    }
}

void ATitanCharacter::Multicast_PlayJobMontage_Implementation(FName SectionName) 
{ 
    ProcessSkill(TEXT("JobAbility"), SectionName); 
}

void ATitanCharacter::Server_ThrowTarget_Implementation(FVector ThrowStartLocation)
{
    if (!bIsGrabbing || !GrabbedActor) return;

    ForceRecoverState();

    StopAnimMontage();

    GetWorldTimerManager().ClearTimer(HitLoopTimerHandle);
    GetWorldTimerManager().ClearTimer(AttackHitTimerHandle);
    GetWorldTimerManager().ClearTimer(MeleeStopTimerHandle); 

    SwingDamagedActors.Empty();

    GetWorldTimerManager().ClearTimer(GrabTimerHandle);
    Multicast_PlayJobMontage(TEXT("Throw"));

    bIsAttacking = false;

    FRotator ThrowRotation = GetControlRotation();
    FVector ThrowDir = ThrowRotation.Vector();
    
    if (ThrowDir.Z < -0.1f) { ThrowDir.Z = -0.1f; ThrowDir.Normalize(); }
    
    // Z Bias 적용하여 궤적 각도 보정
    FVector FinalThrowDir = (ThrowDir + FVector(0.f, 0.f, TrajectoryZBias)).GetSafeNormal();
    FVector BaseVelocity = FinalThrowDir * ThrowForce;

    // 2. 시작 위치 보정
    FVector ExactStartLoc = GetActorLocation(); 
    FName SocketName = TEXT("Hand_R_Socket");
    if (GetMesh() && GetMesh()->DoesSocketExist(SocketName))
    {
        ExactStartLoc = GetMesh()->GetSocketLocation(SocketName) + (GetActorForwardVector() * 30.0f);
    }

    // [CASE A] 캐릭터 던지기
    if (ACharacter* Victim = Cast<ACharacter>(GrabbedActor))
    {
       Victim->SetActorLocation(ExactStartLoc, false, nullptr, ETeleportType::TeleportPhysics);

       Victim->SetReplicateMovement(true);
       
       Multicast_ForceThrowCleanup(Victim, BaseVelocity);

       // 캐릭터 공기 저항 및 마찰력 제거 (투사체처럼 날아가도록)
       if (Victim->GetCharacterMovement())
       {
           Victim->GetCharacterMovement()->BrakingDecelerationFalling = 0.0f; 
           Victim->GetCharacterMovement()->GravityScale = 1.0f;
           Victim->GetCharacterMovement()->AirControl = 0.0f; 
           Victim->GetCharacterMovement()->FallingLateralFriction = 0.0f;
       }

       // 캐릭터용 힘 보정 적용
       Victim->LaunchCharacter(BaseVelocity * ThrowCharacterMultiplier, true, true);
       
       Victim->OnActorHit.RemoveDynamic(this, &ATitanCharacter::OnThrownActorHit);
       Victim->OnActorHit.AddDynamic(this, &ATitanCharacter::OnThrownActorHit);
       
       FTimerHandle RecoveryHandle;
       FTimerDelegate Delegate;
       Delegate.BindUFunction(this, TEXT("RecoverCharacter"), Victim);
       GetWorldTimerManager().SetTimer(RecoveryHandle, Delegate, 2.0f, false);
    }
    // [CASE B] 사물 던지기
    else if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(GrabbedActor->GetRootComponent()))
    {
       GrabbedActor->SetActorLocation(ExactStartLoc, false, nullptr, ETeleportType::TeleportPhysics);

       Multicast_ForceThrowCleanup(GrabbedActor, BaseVelocity);
       GrabbedActor->SetReplicateMovement(true); 
       
       RootComp->SetCollisionProfileName(TEXT("PhysicsActor"));
       RootComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
       RootComp->SetSimulatePhysics(true);
       RootComp->WakeAllRigidBodies();
       
       // 공기 저항 제거
       RootComp->SetLinearDamping(0.0f); 
       RootComp->SetAngularDamping(0.05f); 

       // 자기 자신과의 충돌 무시
       if (GetCapsuleComponent())
       {
          GetCapsuleComponent()->IgnoreComponentWhenMoving(RootComp, true);
          RootComp->IgnoreComponentWhenMoving(GetCapsuleComponent(), true);
       }

       if (GrabbedActor->IsA(ATitanThrowableActor::StaticClass()))
       {
           GrabbedActor->SetLifeSpan(5.0f);
       }

       RootComp->SetPhysicsLinearVelocity(BaseVelocity); 

       GrabbedActor->OnActorHit.RemoveDynamic(this, &ATitanCharacter::OnThrownActorHit);
       GrabbedActor->OnActorHit.AddDynamic(this, &ATitanCharacter::OnThrownActorHit);

       if (ATitanThrowableActor* Throwable = Cast<ATitanThrowableActor>(GrabbedActor))
       {
          Throwable->OnThrown(FinalThrowDir);
       }
    }

    Multicast_OnThrowComplete();

    // 상태 초기화
    GrabbedActor = nullptr;
    bIsGrabbing = false;
    bIsCooldown = true;
    GetWorldTimerManager().SetTimer(CooldownTimerHandle, this, &ATitanCharacter::ResetCooldown, ThrowCooldownTime, false);

	TriggerSkillCooldown(3, ThrowCooldownTime);
}

void ATitanCharacter::Multicast_OnThrowComplete_Implementation()
{
    bIsGrabbing = false;
    GrabbedActor = nullptr;
    bIsAttacking = false; // 던지기 액션 종료 후 공격 가능 상태로 복구
    ShowTrajectory(false);
}

void ATitanCharacter::Server_Skill1_Implementation()
{
    if (bIsDead || IsValid(HeldRock)) return;

    ForceRecoverState();

    if (!SkillDataTable) return;

    static const FString ContextString(TEXT("TitanSkill1"));
    FSkillData* Row = SkillDataTable->FindRow<FSkillData>(TEXT("Skill1"), ContextString);

    if (Row)
    {
       if (!IsSkillUnlocked(Row->RequiredStage)) return;
       CurrentSkillDamage = Row->GetRandomizedDamage();
       if (Row->Cooldown > 0.0f) Skill1CooldownTime = Row->Cooldown;
       Multicast_Skill1();

       TriggerSkillCooldown(1, Skill1CooldownTime);
    }

    bIsSkill1Cooldown = true;
    GetWorldTimerManager().SetTimer(Skill1CooldownTimerHandle, this, &ATitanCharacter::ResetSkill1Cooldown, Skill1CooldownTime, false);
}

void ATitanCharacter::Multicast_Skill1_Implementation()
{
    ProcessSkill(TEXT("Skill1"));
}

void ATitanCharacter::ThrowRock()
{
    if (!HeldRock) return;

    HeldRock->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(HeldRock->GetRootComponent()))
    {
       RootComp->SetSimulatePhysics(true);
       RootComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

       FVector ThrowDir = GetControlRotation().Vector();
       ThrowDir = (ThrowDir + FVector(0, 0, 0.2f)).GetSafeNormal();

       RootComp->AddImpulse(ThrowDir * RockThrowForce * RootComp->GetMass());
    }
    HeldRock = nullptr;
}

void ATitanCharacter::Multicast_SpawnRockEffects_Implementation(FVector SpawnLocation)
{
    FVector ForwardOffset = GetActorForwardVector() * 150.0f;
    SpawnLocation += ForwardOffset;

    if (GroundCrackDecal)
    {

        FVector DecalSize = FVector(130.0f, 130.0f, 130.0f);

        FRotator DecalRot = FRotator(-90.0f, 0.0f, 0.0f);
        DecalRot.Roll = FMath::RandRange(0.0f, 360.0f);


        UGameplayStatics::SpawnDecalAtLocation(
            GetWorld(),
            GroundCrackDecal,
            DecalSize,
            SpawnLocation,
            DecalRot,
            2.0f // 수명 (LifeSpan)
        );
    }

    if (DebrisClass) // RockClass 대신 새로 만든 DebrisClass를 사용합니다.
    {
        int32 DebrisCount = FMath::RandRange(3, 5);
        for (int32 i = 0; i < DebrisCount; i++)
        {
            FVector RandomOffset = FMath::VRand();
            RandomOffset.Z = FMath::Abs(RandomOffset.Z);
            RandomOffset *= FMath::RandRange(50.0f, 150.0f);
            FVector DebrisLoc = SpawnLocation + RandomOffset;
            FRotator DebrisRot = FRotator(FMath::RandRange(0, 360), FMath::RandRange(0, 360), 0);

            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            // 파편 스폰! (설정이 BP에 다 되어 있으니 일반 스폰으로 충분합니다)
            AActor* Debris = GetWorld()->SpawnActor<AActor>(DebrisClass, DebrisLoc, DebrisRot, SpawnParams);

            if (Debris)
            {
                // 크기 랜덤 설정
                float RandomScale = FMath::RandRange(0.01f, 0.1f);
                Debris->SetActorScale3D(FVector(RandomScale));

                // 블루프린트의 메쉬 컴포넌트를 가져와서 튀어 오르는 힘만 줍니다.
                if (UStaticMeshComponent* MeshComp = Debris->FindComponentByClass<UStaticMeshComponent>())
                {
                    float ZForce = FMath::FRandRange(1.0f, 1.5f);
                    FVector LaunchDir = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), ZForce).GetSafeNormal();

                    MeshComp->AddImpulse(LaunchDir * 200.0f * MeshComp->GetMass());
                    MeshComp->AddAngularImpulseInDegrees(FMath::VRand() * 1000.0f, NAME_None, true);
                }
            }
        }
    }
}

// =================================================================
// [돌(Rock) 스킬 관련]
// =================================================================

void ATitanCharacter::ExecuteSpawnRock()
{
    Server_SpawnRock();
}

void ATitanCharacter::Server_SpawnRock_Implementation()
{
    if (HeldRock)
    {
        if (IsValid(HeldRock)) return;
        else HeldRock = nullptr;
    }

    if (RockClass)
    {
       FName SocketName = TEXT("Rock_Socket");
       if (!GetMesh()->DoesSocketExist(SocketName))
       {
          SocketName = TEXT("Hand_R_Socket");
       }

       FVector SocketLoc = GetMesh()->GetSocketLocation(SocketName);
       FRotator SocketRot = GetMesh()->GetSocketRotation(SocketName);

       FActorSpawnParameters SpawnParams;
       SpawnParams.Owner = this;
       SpawnParams.Instigator = this;

       HeldRock = GetWorld()->SpawnActor<ATitanRock>(RockClass, SocketLoc, SocketRot, SpawnParams);

       if (HeldRock)
       {
          HeldRock->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);

          if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(HeldRock->GetRootComponent()))
          {
             RootComp->SetSimulatePhysics(false);
             RootComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
          }
          HeldRock->InitializeRock(CurrentSkillDamage, this);

          FVector EffectLoc = GetActorLocation();
          EffectLoc.Z -= 90.0f; // 발바닥 높이
          Multicast_SpawnRockEffects(EffectLoc);
       }
    }
}

void ATitanCharacter::ExecuteThrowRock()
{
    Server_ThrowRock();
}

void ATitanCharacter::Server_ThrowRock_Implementation()
{
    if (!HeldRock) return;

    if (!IsValid(HeldRock))
    {
        HeldRock = nullptr;
        return;
    }

    HeldRock->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(HeldRock->GetRootComponent()))
    {
       RootComp->SetSimulatePhysics(true);
       RootComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

       if (GetCapsuleComponent())
       {
           GetCapsuleComponent()->IgnoreComponentWhenMoving(RootComp, true);
           RootComp->IgnoreComponentWhenMoving(GetCapsuleComponent(), true);
       }

       FRotator ThrowRot = GetControlRotation();
       FVector ThrowDir = ThrowRot.Vector();
       ThrowDir = (ThrowDir + FVector(0, 0, 0.5f)).GetSafeNormal();

       RootComp->AddImpulse(ThrowDir * RockThrowForce * RootComp->GetMass());
    }

    HeldRock = nullptr;
}

void ATitanCharacter::Server_Skill2_Implementation()
{
    if (bIsCharging || bIsDead) return;

    ForceRecoverState();

    if (!SkillDataTable) return;

    static const FString ContextString(TEXT("Skill2 Context"));
    FSkillData* Row = SkillDataTable->FindRow<FSkillData>(TEXT("Skill2"), ContextString);

    if (Row)
    {
       if (!IsSkillUnlocked(Row->RequiredStage)) return;
       CurrentSkillDamage = Row->GetRandomizedDamage();
       if (Row->Cooldown > 0.0f) Skill2CooldownTime = Row->Cooldown;
       Multicast_Skill2();
    	
    	TriggerSkillCooldown(2, Skill2CooldownTime);
    }

    if (!bIsCharging)
    {
       bIsCharging = true;

       Multicast_ToggleDashFX(true);

       HitVictims.Empty();

       DefaultGroundFriction = GetCharacterMovement()->GroundFriction;
       DefaultGravityScale = GetCharacterMovement()->GravityScale;

       GetCharacterMovement()->GroundFriction = 0.0f;
       GetCharacterMovement()->GravityScale = 0.0f;
       GetCharacterMovement()->BrakingDecelerationFlying = 0.0f;

       GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
       GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
       GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);

       FVector LaunchDir = GetActorForwardVector();
       LaunchCharacter(LaunchDir * 3000.f, true, true);

       bIsSkill2Cooldown = true;
       GetWorldTimerManager().SetTimer(Skill2CooldownTimerHandle, this, &ATitanCharacter::ResetSkill2Cooldown, Skill2CooldownTime, false);
       GetWorldTimerManager().SetTimer(ChargeTimerHandle, this, &ATitanCharacter::StopCharge, 0.3f, false);
    }
}

void ATitanCharacter::Multicast_Skill2_Implementation()
{
    ProcessSkill(TEXT("Skill2"));
}

void ATitanCharacter::OnChargeOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!HasAuthority() || !bIsCharging || OtherActor == this || HitVictims.Contains(OtherActor)) return;

    if (ACharacter* VictimChar = Cast<ACharacter>(OtherActor))
    {
        GetCapsuleComponent()->IgnoreActorWhenMoving(VictimChar, true);
        VictimChar->GetCapsuleComponent()->IgnoreActorWhenMoving(this, true);
        HitVictims.Add(VictimChar);

        // 데미지 적용
        if (!VictimChar->ActorHasTag("Player"))
        {
            UGameplayStatics::ApplyDamage(OtherActor, CurrentSkillDamage, GetController(), this, UDamageType::StaticClass());
        }

        // [핵심 로직] 보스 판별 (상속 구조 활용)
        bool bIsBoss = VictimChar->IsA(AEnemyCharacterBase::StaticClass()) && !VictimChar->IsA(AEnemyNormal::StaticClass());

        if (!bIsBoss)
        {
            FVector KnockbackDir = GetActorForwardVector();
            FVector LaunchForce = (KnockbackDir * 500.f) + FVector(0, 0, 1000.f);
            VictimChar->LaunchCharacter(LaunchForce, true, true);
        }
    }
}

void ATitanCharacter::OnChargeHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    if (!HasAuthority() || !bIsCharging) return;
    if (OtherActor->IsRootComponentStatic())
    {
       StopCharge();
    }
}

void ATitanCharacter::Multicast_ToggleDashFX_Implementation(bool bActive)
{
    // 1. 나이아가라 컴포넌트가 없으면 생성
    if (!DashPSC && DashWindEffect)
    {
        DashPSC = UNiagaraFunctionLibrary::SpawnSystemAttached(
            DashWindEffect,
            GetMesh(),
            TEXT("spine_03"), // 등짝에 붙이기
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            EAttachLocation::SnapToTarget,
            false // AutoDestroy (계속 껐다 켰다 할 거라 false)
        );

        // 처음엔 꺼둠
        if (DashPSC) DashPSC->Deactivate();
    }

    // 2. 켜고 끄기
    if (DashPSC)
    {
        if (bActive)
        {
            DashPSC->Activate(); // 켜기
        }
        else
        {
            DashPSC->Deactivate(); // 끄기
        }
    }

    // 3. 카메라 줌 아웃 (변수 이름 확인: Camera vs FollowCamera)
    if (IsLocallyControlled() && Camera)
    {
        if (bActive) Camera->SetFieldOfView(105.0f);
        else Camera->SetFieldOfView(90.0f);
    }
}

void ATitanCharacter::ProcessSkill(FName SkillRowName, FName StartSectionName)
{
    if (!SkillDataTable) return;
    static const FString ContextString(TEXT("TitanSkillContext"));
    FSkillData* Data = SkillDataTable->FindRow<FSkillData>(SkillRowName, ContextString);

    if (Data && Data->SkillMontage)
    {
        if (!IsSkillUnlocked(Data->RequiredStage)) return;
       UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
       if (AnimInstance)
       {
          if (AnimInstance->Montage_IsPlaying(Data->SkillMontage) && StartSectionName != NAME_None)
          {
             AnimInstance->Montage_JumpToSection(StartSectionName, Data->SkillMontage);
          }
          else
          {
             PlayActionMontage(Data->SkillMontage);
             if (StartSectionName != NAME_None)
             {
                AnimInstance->Montage_JumpToSection(StartSectionName, Data->SkillMontage);
             }
          }
       }
    }
}

void ATitanCharacter::StopCharge()
{
    bIsCharging = false;
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->GroundFriction = DefaultGroundFriction;
    GetCharacterMovement()->GravityScale = DefaultGravityScale;

    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);

    Multicast_ToggleDashFX(false);

    for (AActor* IgnoredActor : HitVictims)
    {
       if (IgnoredActor)
       {
          GetCapsuleComponent()->IgnoreActorWhenMoving(IgnoredActor, false);
          if (ACharacter* IgnoredChar = Cast<ACharacter>(IgnoredActor))
          {
             IgnoredChar->GetCapsuleComponent()->IgnoreActorWhenMoving(this, false);
          }
       }
    }
    HitVictims.Empty();
}

void ATitanCharacter::UpdateHoverHighlight()
{
    // [안전장치] 타이탄도 내 캐릭터 아니면 감지 로직 돌리지 않음
    if (!IsLocallyControlled()) return;

    if (!Camera) return;

    FVector Start = Camera->GetComponentLocation();
    FVector End = Start + (Camera->GetForwardVector() * 600.0f);

    FCollisionShape Shape = FCollisionShape::MakeSphere(70.0f);

    TArray<FHitResult> HitResults;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
    ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
    ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

    bool bHit = GetWorld()->SweepMultiByObjectType(
        HitResults, Start, End, FQuat::Identity, ObjectParams, Shape, Params
    );

    AActor* NewTarget = nullptr;

    if (bHit)
    {
        for (const FHitResult& Hit : HitResults)
        {
            AActor* HitActor = Hit.GetActor();
            if (!HitActor) continue;

            // =========================================================
            // [화이트리스트] 타이탄이 상호작용 가능한 것만!
            // =========================================================

            // 1. 던질 수 있는 물건 (바위, 버섯 등)
            bool bIsThrowable = (Cast<ATitanThrowableActor>(HitActor) != nullptr);

            // 2. 플레이어 (메이지를 잡을 수 있으므로 포함)
            bool bIsPlayer = (Cast<ABaseCharacter>(HitActor) != nullptr);

            // 3. 일반 몬스터
            bool bIsNormalEnemy = (Cast<AEnemyNormal>(HitActor) != nullptr);

            // 기둥(Pillar) 같은 건 여기 없으므로 타이탄은 무시하게 됨 -> 하이라이트 안 됨
            if (bIsThrowable || bIsPlayer || bIsNormalEnemy)
            {
                NewTarget = HitActor;
                break;
            }
        }
    }

    // 하이라이트 갱신
    if (HoveredActor != NewTarget)
    {
        if (HoveredActor) SetHighlight(HoveredActor, false); // 끄기
        if (NewTarget) SetHighlight(NewTarget, true); // 켜기
        HoveredActor = NewTarget;
    }
}

void ATitanCharacter::SetHighlight(AActor* Target, bool bEnable)
{
    if (!Target) return;
    TArray<UPrimitiveComponent*> Components;
    Target->GetComponents<UPrimitiveComponent>(Components);

    for (auto Comp : Components)
    {
        Comp->SetRenderCustomDepth(bEnable);
        if (bEnable)
        {
            Comp->SetCustomDepthStencilValue(250);
        }
    }
}

void ATitanCharacter::RecoverCharacter(ACharacter* Victim)
{
    if (!Victim || !Victim->IsValidLowLevel()) return;

    Multicast_FixMesh(Victim);
    if (Victim->GetCapsuleComponent()) 
       Victim->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    if (Victim->GetCharacterMovement())
    {
       Victim->GetCharacterMovement()->StopMovementImmediately();
       Victim->GetCharacterMovement()->Velocity = FVector::ZeroVector;

       // 던져질 때 해제했던 물리/이동 제약 복구
       Victim->GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f; 
       Victim->GetCharacterMovement()->GravityScale = 1.0f; 
       Victim->GetCharacterMovement()->AirControl = 0.05f;  

       FVector FixLoc = Victim->GetActorLocation() + FVector(0.f, 0.f, 5.0f);
       Victim->SetActorLocation(FixLoc, false, nullptr, ETeleportType::TeleportPhysics);
       Victim->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }

    Victim->ForceNetUpdate();
    AAIController* AIC = Cast<AAIController>(Victim->GetController());
    if (AIC && AIC->GetBrainComponent()) AIC->GetBrainComponent()->RestartLogic();
}

void ATitanCharacter::SetHeldState(ACharacter* Target, bool bIsHeld)
{
    if (!Target) return;

    AController* TargetCon = Target->GetController();
    AAIController* AIC = Cast<AAIController>(TargetCon);
    UCharacterMovementComponent* CMC = Target->GetCharacterMovement();
    UCapsuleComponent* Capsule = Target->GetCapsuleComponent();

    if (bIsHeld)
    {
       Target->StopAnimMontage();
       if (CMC)
       {
          CMC->StopMovementImmediately();
          CMC->DisableMovement();
       }
       if (Capsule) Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
       if (AIC && AIC->GetBrainComponent())
       {
          AIC->GetBrainComponent()->StopLogic(TEXT("Grabbed"));
          AIC->StopMovement();
       }
    }
    else
    {
       if (Capsule) Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
       if (CMC) CMC->SetMovementMode(MOVE_Falling);
    }
}

void ATitanCharacter::ResetCooldown()
{
    bIsCooldown = false;
}

void ATitanCharacter::ResetSkill1Cooldown()
{
    bIsSkill1Cooldown = false;
}

void ATitanCharacter::ResetSkill2Cooldown()
{
    bIsSkill2Cooldown = false;
}

void ATitanCharacter::ShowTrajectory(bool bShow)
{
    if (TrajectorySpline) TrajectorySpline->SetVisibility(bShow);

    for (USplineMeshComponent* SplineMesh : SplineMeshes)
    {
       if (SplineMesh) SplineMesh->SetVisibility(bShow);
    }
}

void ATitanCharacter::UpdateTrajectory()
{
    // 잡은 대상이나 들고 있는 바위가 없으면 궤적 계산 안함
    if (!TrajectorySpline) return;
    if (!GrabbedActor && !HeldRock) return; 

    // 1. [물리 예측 계산]
    FPredictProjectilePathParams PredictParams;

    FName SocketName = TEXT("Hand_R_Socket");

    if (GetMesh() && GetMesh()->DoesSocketExist(SocketName))
    {
        // 손 위치에서 약간 앞쪽으로 보정하여 시작점 설정
        FVector HandLoc = GetMesh()->GetSocketLocation(SocketName);
        FVector ForwardOffset = GetActorForwardVector() * 30.0f; 
        PredictParams.StartLocation = HandLoc + ForwardOffset; 
    }
    else
    {
        PredictParams.StartLocation = ThrowSpawnPoint->GetComponentLocation();
    }
    
    // 자신과 잡고 있는 물체는 충돌 검사에서 제외
    PredictParams.ActorsToIgnore.Add(this);
    PredictParams.ActorsToIgnore.Add(GrabbedActor); 
    if (HeldRock) PredictParams.ActorsToIgnore.Add(HeldRock);
    
    FRotator ThrowRotation = GetControlRotation();
    FVector ThrowDir = ThrowRotation.Vector();
    if (ThrowDir.Z < -0.1f) { ThrowDir.Z = -0.1f; ThrowDir.Normalize(); }
    
    FVector FinalThrowDir = (ThrowDir + FVector(0.f, 0.f, TrajectoryZBias)).GetSafeNormal();
    
    PredictParams.LaunchVelocity = FinalThrowDir * ThrowForce;
    
    PredictParams.MaxSimTime = 5.0f;       
    PredictParams.ProjectileRadius = 30.0f;      
    PredictParams.SimFrequency = 60.0f;    

    // 중력 보정 적용
    PredictParams.OverrideGravityZ = GetWorld()->GetGravityZ() * TrajectoryGravityScale;
    
    PredictParams.bTraceWithCollision = true; 
    PredictParams.bTraceWithChannel = true;
    PredictParams.TraceChannel = ECC_WorldStatic; 
    PredictParams.DrawDebugType = EDrawDebugTrace::None; 

    FPredictProjectilePathResult PredictResult;
    UGameplayStatics::PredictProjectilePath(this, PredictParams, PredictResult);


    // 2. [스플라인 포인트 업데이트]
    TrajectorySpline->ClearSplinePoints(false);
    
    for (const FPredictProjectilePathPointData& PointData : PredictResult.PathData)
    {
        TrajectorySpline->AddSplinePoint(PointData.Location, ESplineCoordinateSpace::World, false);
    }
    TrajectorySpline->UpdateSpline();


    // 3. [시각화: 스플라인 메쉬 배치 (풀링)]
    const int32 PointCount = TrajectorySpline->GetNumberOfSplinePoints();
    
    for (int32 i = 0; i < PointCount - 1; i++)
    {
        if (SplineMeshes.Num() <= i)
        {
            USplineMeshComponent* NewMesh = NewObject<USplineMeshComponent>(this);
            NewMesh->SetMobility(EComponentMobility::Movable);
            NewMesh->SetupAttachment(TrajectorySpline);
            
            if (TrajectoryMesh) NewMesh->SetStaticMesh(TrajectoryMesh);
            if (TrajectoryMaterial) NewMesh->SetMaterial(0, TrajectoryMaterial);
            
            NewMesh->RegisterComponent();
            SplineMeshes.Add(NewMesh);
        }

        USplineMeshComponent* CurrentMesh = SplineMeshes[i];
        CurrentMesh->SetVisibility(true);

        FVector StartLoc, StartTan, EndLoc, EndTan;
        TrajectorySpline->GetLocationAndTangentAtSplinePoint(i, StartLoc, StartTan, ESplineCoordinateSpace::Local);
        TrajectorySpline->GetLocationAndTangentAtSplinePoint(i + 1, EndLoc, EndTan, ESplineCoordinateSpace::Local);

        FVector2D ScaleValue = FVector2D(0.04f, 0.04f); 
        CurrentMesh->SetStartScale(ScaleValue);
        CurrentMesh->SetEndScale(ScaleValue);

        CurrentMesh->SetStartAndEnd(StartLoc, StartTan, EndLoc, EndTan, true);
    }
    
    // 사용하지 않는 메쉬 숨기기
    for (int32 i = PointCount - 1; i < SplineMeshes.Num(); i++)
    {
        SplineMeshes[i]->SetVisibility(false);
    }
}

void ATitanCharacter::ForceRecoverState()
{
    // 1. 모든 근접 공격 타이머 및 판정 초기화
    StopMeleeTrace();
    GetWorldTimerManager().ClearTimer(AttackHitTimerHandle);
    GetWorldTimerManager().ClearTimer(HitLoopTimerHandle);
    GetWorldTimerManager().ClearTimer(MeleeStopTimerHandle);
    SwingDamagedActors.Empty();

    GetWorldTimerManager().ClearTimer(AttackCooldownTimerHandle);
    bIsAttackCoolingDown = false;
    bIsAttacking = false;

    // 3. 잡기 상태 복구: 잡고 있는 대상이 유효하지 않으면 상태 초기화
    if (bIsGrabbing && !IsValid(GrabbedActor))
    {
        bIsGrabbing = false;
        GrabbedActor = nullptr;
        ShowTrajectory(false);
    }

    // 4. 돌진 스킬 중이었다면 중단
    if (bIsCharging)
    {
        StopCharge();
    }

    // 5. 무브먼트 상태 정상화
    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        if (MoveComp->MovementMode == MOVE_Flying)
        {
            MoveComp->SetMovementMode(MOVE_Falling);
        }
        MoveComp->GroundFriction = 8.0f;
        MoveComp->GravityScale = 1.0f;
    }
}

void ATitanCharacter::OnMontageEndedDelegate(UAnimMontage* Montage, bool bInterrupted)
{
    if (GetWorldTimerManager().IsTimerActive(HitLoopTimerHandle))
    {
        StopMeleeTrace();
    }

    if (bIsCharging)
    {
        StopCharge();
    }

    if (bInterrupted && IsValid(HeldRock))
    {
        HeldRock->Destroy();
        HeldRock = nullptr;
    }

    bIsAttacking = false;

    if (!GrabbedActor || !IsValid(GrabbedActor))
    {
        bIsGrabbing = false;
        ShowTrajectory(false); 
    }
}