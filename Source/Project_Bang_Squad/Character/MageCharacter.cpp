#include "Project_Bang_Squad/Character/MageCharacter.h"
#include "Project_Bang_Squad/Projectile/MageProjectile.h"
#include "Project_Bang_Squad/Character/Player/Mage/MageSkill2Rock.h"
#include "Project_Bang_Squad/Character/Pillar.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h" 
#include "Particles/ParticleSystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Components/TimelineComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Curves/CurveFloat.h"
#include "NiagaraFunctionLibrary.h" 
#include "NiagaraComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/Mage/MageSkill2Rock.h"
#include "Player/Mage/MagicBoat.h"
#include "Player/Mage/MovePillar.h"

// ====================================================================================
// [Section 1] 초기화 (Initialization)
// ====================================================================================

AMageCharacter::AMageCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // 시점 설정 (카메라 회전에 캐릭터가 즉각 동기화되지 않음)
    bUseControllerRotationYaw = true;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;

    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
       MoveComp->MaxWalkSpeed = 550.f;
       MoveComp->bOrientRotationToMovement = false; 
       MoveComp->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
    }

    // 기본 스탯 초기화
    AttackCooldownTime = 1.f;
    UnlockedStageLevel = 1;
    FocusedPillar = nullptr;
    CurrentTargetPillar = nullptr;

    // --- 컴포넌트 생성 ---
    
    // 1. 기믹 조작용 카메라 타임라인
    CameraTimelineComp = CreateDefaultSubobject<UTimelineComponent>(TEXT("CameraTimelineComp"));

    // 2. 보트 탑승용 탑다운 카메라 시스템
    TopDownSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("TopDownSpringArm"));
    TopDownSpringArm->SetupAttachment(GetRootComponent());
    TopDownSpringArm->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f)); 
    TopDownSpringArm->TargetArmLength = 1200.0f; 
    TopDownSpringArm->bDoCollisionTest = false; 

    TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
    TopDownCamera->SetupAttachment(TopDownSpringArm, USpringArmComponent::SocketName);
    TopDownCamera->SetAutoActivate(false);

    // 3. 기본 카메라 조정
    if (SpringArm)
    {
       SpringArm->TargetOffset = FVector(0.0f, 0.0f, 150.0f); 
       SpringArm->ProbeSize = 12.0f;
       SpringArm->bUsePawnControlRotation = true; 
    }

    // 4. VFX 컴포넌트
    JobAuraComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("JobAuraComp"));
    JobAuraComp->SetupAttachment(GetMesh(), TEXT("Bip001-Pelvis")); 
    JobAuraComp->bAutoActivate = false; 

    // 5. 무기 소켓 및 악세서리 부착점 설정
    WeaponRootComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Weapon_Root_R"));
    WeaponRootComp->SetupAttachment(GetMesh(), TEXT("Weapon_Root_R")); 
    ItemAttachParent = WeaponRootComp; 
    AccessorySocketName = FName("Acc_Socket");

    if (HeadAccessoryComponent)
    {
       HeadAccessoryComponent->SetupAttachment(WeaponRootComp);
       HeadAccessoryComponent->SetRelativeLocation(FVector::ZeroVector);
       HeadAccessoryComponent->SetRelativeRotation(FRotator::ZeroRotator);
    }
    if (HeadSkeletalComp)
    {
       HeadSkeletalComp->SetupAttachment(WeaponRootComp);
       HeadSkeletalComp->SetRelativeLocation(FVector::ZeroVector);
       HeadSkeletalComp->SetRelativeRotation(FRotator::ZeroRotator);
    }
}

void AMageCharacter::BeginPlay()
{
    Super::BeginPlay();

    // 카메라 기본 설정 고정
    if (SpringArm)
    {
       SpringArm->bUsePawnControlRotation = true;
       SpringArm->bInheritPitch = true;
       SpringArm->bInheritYaw = true;
       SpringArm->bInheritRoll = false;

       DefaultArmLength = SpringArm->TargetArmLength;
       DefaultSocketOffset = SpringArm->SocketOffset;
    }

    if (TopDownCamera) TopDownCamera->SetActive(false);

    // --- 기둥 줌인 타임라인 바인딩 ---
    FOnTimelineFloat TimelineProgress;
    TimelineProgress.BindDynamic(this, &AMageCharacter::CameraTimelineProgress);
    if (CameraCurve)
    {
       CameraTimelineComp->AddInterpFloat(CameraCurve, TimelineProgress);
    }
    else
    {
       CameraTimelineComp->AddInterpFloat(nullptr, TimelineProgress);
       CameraTimelineComp->SetTimelineLength(1.0f);
    }

    FOnTimelineEvent TimelineFinishedEvent;
    TimelineFinishedEvent.BindDynamic(this, &AMageCharacter::OnCameraTimelineFinished);
    CameraTimelineComp->SetTimelineFinishedFunc(TimelineFinishedEvent);
    CameraTimelineComp->SetLooping(false);

    // --- 무기 메쉬 캐싱 및 VFX 설정 ---
    TArray<UStaticMeshComponent*> StaticComps;
    GetComponents<UStaticMeshComponent>(StaticComps);
    for (UStaticMeshComponent* Comp : StaticComps)
    {
       if (Comp && (Comp->DoesSocketExist(TEXT("Weapon_Root_R")) || Comp->DoesSocketExist(TEXT("TipSocket")))) 
       { 
          CachedWeaponMesh = Comp; 
          break; 
       }
    }

    if (CachedWeaponMesh && StaffTrailVFX)
    {
       StaffTrailComp = UNiagaraFunctionLibrary::SpawnSystemAttached(StaffTrailVFX, CachedWeaponMesh, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, false);
       if (StaffTrailComp) StaffTrailComp->Deactivate(); 
    }
    
    if (JobAuraComp && JobAuraVFX)
    {
       JobAuraComp->SetAsset(JobAuraVFX);
       JobAuraComp->SetRelativeScale3D(JobAuraScale); 
       JobAuraComp->Deactivate(); 
    }
    
    // --- 스킬 데이터 캐싱 ---
    if (SkillDataTable)
    {
       TArray<FName> RowNames = SkillDataTable->GetRowNames();
       for (const FName& RowName : RowNames)
       {
          static const FString ContextString(TEXT("MageSkillCacheContext"));
          FSkillData* Data = SkillDataTable->FindRow<FSkillData>(RowName, ContextString);
          if (Data) SkillDataCache.Add(RowName, Data);
       }
    }

    // 로컬 클라이언트에서만 상호작용 감지 타이머 실행
    if (IsLocallyControlled())
    {
       GetWorldTimerManager().SetTimer(InteractionTimerHandle, this, &AMageCharacter::CheckInteractableTarget, 0.1f, true);
    }
}

void AMageCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
       if (JobAbilityAction)
       {
          EIC->BindAction(JobAbilityAction, ETriggerEvent::Started, this, &AMageCharacter::JobAbility); 
          EIC->BindAction(JobAbilityAction, ETriggerEvent::Completed, this, &AMageCharacter::EndJobAbility); 
          EIC->BindAction(JobAbilityAction, ETriggerEvent::Canceled, this, &AMageCharacter::EndJobAbility); 
       }
    }
}

// ====================================================================================
// [Section 2] 상태 관리 및 틱 (State & Tick)
// ====================================================================================

void AMageCharacter::OnDeath()
{
    if (bIsDead) return;
    
    Server_SetAuraActive(false);

    if (bIsPillarMode || bIsBoatMode) EndJobAbility();

    GetWorldTimerManager().ClearTimer(ComboResetTimer);
    GetWorldTimerManager().ClearTimer(ProjectileTimerHandle);
    StopAnimMontage();

    Super::OnDeath();
}

void AMageCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (CameraTimelineComp)
    {
       CameraTimelineComp->TickComponent(DeltaTime, ELevelTick::LEVELTICK_TimeOnly, nullptr);
    }

    // 기둥이 무너지면 조종 상태 해제
    if (bIsPillarMode)
    {
       if (!IsValid(CurrentTargetPillar) || CurrentTargetPillar->bIsFallen)
       {
          EndJobAbility();
          return;
       }
    }

    // 조종 중인 기둥으로 시점 부드럽게 고정
    if (bIsPillarMode && IsValid(CurrentTargetPillar) && !CurrentTargetPillar->bIsFallen)
    {
       LockOnPillar(DeltaTime);
    }
}

// ====================================================================================
// [Section 3] 조작 및 상호작용 (Input & Interaction)
// ====================================================================================

void AMageCharacter::CheckInteractableTarget()
{
    if (!IsLocallyControlled())
    {
       GetWorldTimerManager().ClearTimer(InteractionTimerHandle);
       return;
    }

    if (bIsPillarMode || bIsBoatMode) return;

    // 구조물에 매달려 있을 때는 하이라이트 비활성화
    if (GetAttachParentActor() != nullptr)
    {
       if (FocusedPillar)
       {
          if (FocusedPillar->PillarMesh) FocusedPillar->PillarMesh->SetRenderCustomDepth(false);
          FocusedPillar = nullptr;
       }

       if (HoveredActor)
       {
          if (IMagicInteractableInterface* Interface = Cast<IMagicInteractableInterface>(HoveredActor))
             Interface->SetMageHighlight(false);
          HoveredActor = nullptr;
       }
       return; 
    }

    // 전방으로 레이저를 발사하여 상호작용 객체 탐색
    FHitResult HitResult;
    FVector Start = Camera->GetComponentLocation();
    FVector End = Start + (Camera->GetForwardVector() * TraceDistance);
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this); 

    bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params);
    AActor* HitActor = HitResult.GetActor();

    APillar* HitPillar = Cast<APillar>(HitActor);
    bool bIsInterface = (HitActor && HitActor->Implements<UMagicInteractableInterface>());

    // [케이스 1] 쓰러지지 않은 기둥 발견
    if (HitPillar && !HitPillar->bIsFallen)
    {
       if (FocusedPillar != HitPillar)
       {
          if (HoveredActor)
          {
             if (IMagicInteractableInterface* Interface = Cast<IMagicInteractableInterface>(HoveredActor))
                Interface->SetMageHighlight(false);
             HoveredActor = nullptr;
          }
          if (FocusedPillar) FocusedPillar->PillarMesh->SetRenderCustomDepth(false);

          HitPillar->PillarMesh->SetRenderCustomDepth(true);
          HitPillar->PillarMesh->SetCustomDepthStencilValue(255);
          FocusedPillar = HitPillar;
       }
    }
    // [케이스 2] 마법 인터페이스 객체(보트 등) 발견
    else if (bIsInterface)
    {
       if (HoveredActor != HitActor)
       {
          if (FocusedPillar)
          {
             FocusedPillar->PillarMesh->SetRenderCustomDepth(false);
             FocusedPillar = nullptr;
          }
          if (HoveredActor)
          {
             if (IMagicInteractableInterface* Iface = Cast<IMagicInteractableInterface>(HoveredActor))
                Iface->SetMageHighlight(false);
          }
          if (IMagicInteractableInterface* Interface = Cast<IMagicInteractableInterface>(HitActor))
          {
             Interface->SetMageHighlight(true);
          }
          HoveredActor = HitActor;
       }
    }
    // [케이스 3] 허공
    else
    {
       if (FocusedPillar)
       {
          FocusedPillar->PillarMesh->SetRenderCustomDepth(false);
          FocusedPillar = nullptr;
       }
       if (HoveredActor)
       {
          if (IMagicInteractableInterface* Interface = Cast<IMagicInteractableInterface>(HoveredActor))
             Interface->SetMageHighlight(false);
          HoveredActor = nullptr;
       }
    }
}

void AMageCharacter::Look(const FInputActionValue& Value)
{
    FVector2D LookInput = Value.Get<FVector2D>();

    // 1. 보트 또는 회전 기둥 조종 모드
    if (bIsBoatMode && CurrentControlledActor)
    {
       FVector MoveDir = FVector::ZeroVector;

       if (Cast<AMagicBoat>(CurrentControlledActor))
       {
          if (TopDownCamera)
          {
             FVector ScreenUp = TopDownCamera->GetUpVector();
             FVector ScreenRight = TopDownCamera->GetRightVector();
             ScreenUp.Z = 0; 
             ScreenRight.Z = 0;
             ScreenUp.Normalize();
             ScreenRight.Normalize();

             MoveDir = (ScreenUp * LookInput.Y) + (ScreenRight * LookInput.X);
          }
       }
       else
       {
          if (FMath::Abs(LookInput.Y) > 0.1f) 
          {
             MoveDir = FVector(0.f, LookInput.Y, 0.f);
          }
       }

       if (!MoveDir.IsNearlyZero())
       {
          Server_InteractActor(CurrentControlledActor, MoveDir);
       }
    }
    // 2. 파괴형 기둥 조종 모드
    else if (bIsPillarMode)
    {
       if (IsValid(CurrentTargetPillar) && !CurrentTargetPillar->bIsFallen)
       {
          if (FMath::Abs(LookInput.X) > 0.5f && FMath::Sign(LookInput.X) == CurrentTargetPillar->RequiredMouseDirection)
          {
             Server_TriggerPillarFall(CurrentTargetPillar);
             EndJobAbility();
          }
       }
    }
    // 3. 일반 시점 조작
    else
    {
       Super::Look(Value);
    }
}

// ====================================================================================
// [Section 4] 시점 유틸리티 (Camera Utilities)
// ====================================================================================

void AMageCharacter::CameraTimelineProgress(float Alpha)
{
    if (!SpringArm) return;
    float NewArmLength = FMath::Lerp(DefaultArmLength, 1200.f, Alpha);
    SpringArm->TargetArmLength = NewArmLength;
    
    FVector NewOffset = FMath::Lerp(DefaultSocketOffset, FVector::ZeroVector, Alpha);
    SpringArm->SocketOffset = NewOffset;
}

void AMageCharacter::OnCameraTimelineFinished()
{
    if (CameraTimelineComp && CameraTimelineComp->GetPlaybackPosition() <= 0.01f)
    {
       if (APlayerController* PC = Cast<APlayerController>(GetController()))
       {
          PC->bShowMouseCursor = false;
          PC->bEnableClickEvents = false;
          PC->bEnableMouseOverEvents = false;
          FInputModeGameOnly InputMode;
          PC->SetInputMode(InputMode);
       }
    }
}

void AMageCharacter::LockOnPillar(float DeltaTime)
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (PC && CurrentTargetPillar && !CurrentTargetPillar->bIsFallen)
    {
       FVector StartLoc = Camera->GetComponentLocation();
       FVector TargetLoc = CurrentTargetPillar->GetActorLocation() + FVector(0.0f, 0.0f, 300.0f);
       FRotator TargetRot = UKismetMathLibrary::FindLookAtRotation(StartLoc, TargetLoc);
       FRotator NewRot = FMath::RInterpTo(PC->GetControlRotation(), TargetRot, DeltaTime, 5.0f);
       PC->SetControlRotation(NewRot);
    }
}

FVector AMageCharacter::GetCrosshairTargetLocation()
{
    if (!Camera) return GetActorLocation() + GetActorForwardVector() * 1000.f;

    FVector CamLoc = Camera->GetComponentLocation();
    FVector CamForward = Camera->GetForwardVector();

    FVector TraceStart = CamLoc;
    FVector TraceEnd = CamLoc + (CamForward * 10000.0f); 

    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this); 

    // 에임 보정을 위해 두꺼운 구체(Sphere) 레이캐스트 적용
    FCollisionShape Sphere = FCollisionShape::MakeSphere(30.0f);
    bool bHit = GetWorld()->SweepSingleByChannel(HitResult, TraceStart, TraceEnd, FQuat::Identity, ECC_Visibility, Sphere, Params);
   
    return bHit ? HitResult.ImpactPoint : TraceEnd;
}

// ====================================================================================
// [Section 5] 전투 및 스킬 시스템 (Combat System)
// ====================================================================================

void AMageCharacter::Attack()
{
    if (!CanAttack()) return;

    FName SkillName = (CurrentComboIndex == 0) ? TEXT("Attack_A") : TEXT("Attack_B");
    ProcessSkill(SkillName);
    StartAttackCooldown();

    CurrentComboIndex++;
    if (CurrentComboIndex > 1) CurrentComboIndex = 0;

    GetWorldTimerManager().ClearTimer(ComboResetTimer);
    GetWorldTimerManager().SetTimer(ComboResetTimer, this, &AMageCharacter::ResetCombo, 1.2f, false);
}

void AMageCharacter::ResetCombo() { CurrentComboIndex = 0; }
void AMageCharacter::Skill1() { if (!bIsDead) ProcessSkill(TEXT("Skill1")); }
void AMageCharacter::Skill2() { if (!bIsDead) ProcessSkill(TEXT("Skill2")); }

void AMageCharacter::ProcessSkill(FName SkillRowName, FVector TargetLocation)
{
    if (!CanAttack()) return;

    // 로컬 컨트롤러인 경우 크로스헤어가 가리키는 지점을 타겟으로 설정
    if (TargetLocation.IsZero() && IsLocallyControlled())
    {
        TargetLocation = GetCrosshairTargetLocation();
    }

    FSkillData** FoundData = SkillDataCache.Find(SkillRowName);
    FSkillData* Data = (FoundData) ? *FoundData : nullptr;

    if (SkillTimers.Contains(SkillRowName) && GetWorldTimerManager().IsTimerActive(SkillTimers[SkillRowName])) return;

    if (Data)
    {
       if (!IsSkillUnlocked(Data->RequiredStage)) return;

       if (Data->SkillMontage) PlayActionMontage(Data->SkillMontage);

       if (HasAuthority())
       {
          if (Data->SkillMontage) Server_PlayMontage(Data->SkillMontage);

          if (SkillRowName == TEXT("Attack_A") || SkillRowName == TEXT("Attack_B"))
          {
             Multicast_SetTrailActive(true);
          }
          
          // [스킬 2 - 바위 소환]
          if (SkillRowName == TEXT("Skill2")) 
          {
             Multicast_PlaySkill2VFX();
             
             if (Data->ProjectileClass)
             {
                float Dmg = Data->GetRandomizedDamage();
                FTimerDelegate TimerDel;
                TimerDel.BindUObject(this, &AMageCharacter::SpawnSkill2Rock, Data->ProjectileClass.Get(), Dmg);
                
                if (Data->ActionDelay > 0.0f)
                   GetWorldTimerManager().SetTimer(RockSpawnTimerHandle, TimerDel, Data->ActionDelay, false);
                else
                   SpawnSkill2Rock(Data->ProjectileClass, Dmg);
             }
          }
          // [기본 공격 & 스킬 1 - 투사체 발사]
          else
          {
             if (SkillRowName == TEXT("Skill1")) Multicast_PlaySkill1VFX();

             if (Data->ProjectileClass)
             {
                float Dmg = Data->GetRandomizedDamage();
                FTimerDelegate TimerDel;
                TimerDel.BindUObject(this, &AMageCharacter::SpawnDelayedProjectile, Data->ProjectileClass.Get(), Dmg, TargetLocation);

                if (Data->ActionDelay > 0.0f)
                   GetWorldTimerManager().SetTimer(ProjectileTimerHandle, TimerDel, Data->ActionDelay, false);
                else
                   SpawnDelayedProjectile(Data->ProjectileClass, Dmg, TargetLocation);
             }
          }
       }
       else
       {
          // 클라이언트는 서버로 스킬 실행 및 에임 타겟 요청
          Server_ProcessSkill(SkillRowName, TargetLocation);
       }

       // 쿨타임 등록
       if (Data->Cooldown > 0.0f)
       {
          FTimerHandle& Handle = SkillTimers.FindOrAdd(SkillRowName);
          GetWorldTimerManager().SetTimer(Handle, Data->Cooldown, false);
          
          int32 SkillIdx = 0;
          if (SkillRowName == TEXT("Skill1")) SkillIdx = 1;
          else if (SkillRowName == TEXT("Skill2")) SkillIdx = 2;
          
          if (SkillIdx > 0) TriggerSkillCooldown(SkillIdx, Data->Cooldown);
       }
    }
}

void AMageCharacter::SpawnDelayedProjectile(UClass* ProjectileClass, float DamageAmount, FVector TargetLocation)
{
   if (!HasAuthority() || !ProjectileClass) return;
    
   Multicast_SetTrailActive(false);

   // 1. 카메라의 현재 위치와 회전값 획득 (에임 정확도 확보)
   FVector CameraLoc;
   FRotator CameraRot;
   if (APlayerController* PC = Cast<APlayerController>(GetController()))
   {
      PC->GetPlayerViewPoint(CameraLoc, CameraRot);
   }
   else
   {
      CameraLoc = GetActorLocation();
      CameraRot = GetBaseAimRotation();
   }
   
   // 2. 동적 스폰 위치 계산 (TPS 뷰 시차 극복)
   float DistToChar = FVector::Distance(CameraLoc, GetActorLocation());
   FVector SpawnLoc = CameraLoc + (CameraRot.Vector() * (DistToChar + ProjectileForwardOffset)); 
   SpawnLoc.Z -= ProjectileDownwardOffset;
    
   // 3. 크로스헤어 방향으로 최종 각도 도출
   FVector CrosshairTarget = CameraLoc + (CameraRot.Vector() * AimTraceDistance); 
   FRotator SpawnRot = UKismetMathLibrary::FindLookAtRotation(SpawnLoc, CrosshairTarget);

   // ====================================================================
   //  4. 지연 생성 (Deferred Spawning) 적용 - 겹침 버그 완벽 해결
   // ====================================================================
   
   // 스폰 위치와 회전값을 하나의 Transform(변환) 정보로 묶습니다.
   FTransform SpawnTransform(SpawnRot, SpawnLoc);

   // 투사체를 세상에 '절반'만 꺼냅니다. (메모리에는 있지만 충돌 판정은 아직 안 함)
   AMageProjectile* MyProjectile = GetWorld()->SpawnActorDeferred<AMageProjectile>(
      ProjectileClass, 
      SpawnTransform, 
      this,             // Owner
      GetInstigator(),  // Instigator
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn
   );

   if (MyProjectile)
   {
      // 충돌이 일어나기 전(안전한 상태)에 데이터 테이블의 진짜 데미지를 주입합니다!
      MyProjectile->Damage = DamageAmount;
      
      // 데미지 세팅이 끝났으니, 이제 세상에 완전하게 등장시키고 충돌 판정을 시작합니다!
      UGameplayStatics::FinishSpawningActor(MyProjectile, SpawnTransform);
   }
}

void AMageCharacter::SpawnSkill2Rock(UClass* RockClass, float DamageAmount)
{
    Multicast_StopSkill2VFX();

    if (!HasAuthority() || !RockClass) return;
    
    // 에임 지점 근처의 바닥을 정밀하게 탐색
    FVector AimLocation = GetCrosshairTargetLocation();
    FVector TraceStart = AimLocation + FVector(0.f, 0.f, 500.f);
    FVector TraceEnd = AimLocation - FVector(0.f, 0.f, 1000.f);
    
    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    
    FVector FinalSpawnLoc = TraceStart;
    
    if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, Params))
    {
       FinalSpawnLoc = HitResult.ImpactPoint; 
    }
    else
    {
       if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("Skill Failed: No Ground Found!"));
       return;
    }
    
    FRotator SpawnRot = FRotator::ZeroRotator;
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;
    
    AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(RockClass, FinalSpawnLoc, SpawnRot, SpawnParams);
    
    if (AMageSkill2Rock* Rock = Cast<AMageSkill2Rock>(SpawnedActor))
    {
       Rock->InitializeRock(DamageAmount, this);
    }
}

// ====================================================================================
// [Section 6] 직업 능력 (Job Ability)
// ====================================================================================

void AMageCharacter::JobAbility()
{
    if (!CanAttack()) return;
    if (!IsSkillUnlocked(1)) return;
    if (bIsDead) return;

    UAnimMontage* TargetMontage = nullptr;
    FSkillData** FoundData = SkillDataCache.Find(FName("JobAbility"));
    FSkillData* Data = (FoundData) ? *FoundData : nullptr;

    if (Data) TargetMontage = Data->SkillMontage;
    
    bool bSuccess = false;
    
    // [염력 모드 1] 기둥 조작
    if (FocusedPillar)
    {
       if (TargetMontage)
       {
          PlayActionMontage(TargetMontage);
          Server_PlayMontage(TargetMontage);
       }

       bIsPillarMode = true;
       CurrentTargetPillar = FocusedPillar;

       if (SpringArm)
       {
          SpringArm->bUsePawnControlRotation = true;
          SpringArm->bInheritPitch = true;
          SpringArm->bInheritYaw = true;
          SpringArm->bInheritRoll = true;
       }
       if (CameraTimelineComp) CameraTimelineComp->Play(); 
       
       bSuccess = true;
    }
    // [염력 모드 2] 보트 / 회전 기둥 조작
    else if (HoveredActor)
    {
       AMagicBoat* TargetBoat = Cast<AMagicBoat>(HoveredActor);

       if (TargetMontage)
       {
          PlayActionMontage(TargetMontage);
          Server_PlayMontage(TargetMontage);
       }

       // 보트 탑승 여부 검증
       if (TargetBoat)
       {
          UPrimitiveComponent* BaseComponent = GetMovementBase();
          bool bIsRiding = (BaseComponent && BaseComponent->GetOwner() == TargetBoat);
          if (!bIsRiding) return; 
       }

       bIsBoatMode = true;
       CurrentControlledActor = HoveredActor;

       if (Cast<AMovePillar>(HoveredActor))
       {
          Server_InteractActor(CurrentControlledActor, FVector(1.0f, 0.0f, 0.0f));
       }

       // 보트 조작 시 탑다운 카메라 전환
       if (TargetBoat)
       {
          Server_SetBoatRideState(TargetBoat, true);
          GetCharacterMovement()->bEnablePhysicsInteraction = false;

          if (IsLocallyControlled())
          {
             if (Camera) Camera->SetActive(false);
             if (TopDownCamera) TopDownCamera->SetActive(true);
          }
       }
       bSuccess = true;
    }
    
    if (bSuccess) Server_SetAuraActive(true);
}

void AMageCharacter::EndJobAbility()
{
    UAnimMontage* TargetMontage = nullptr;
    FSkillData** FoundData = SkillDataCache.Find(FName("JobAbility"));
    FSkillData* Data = (FoundData) ? *FoundData : nullptr;

    if (Data) TargetMontage = Data->SkillMontage;

    if (TargetMontage)
    {
       StopAnimMontage(TargetMontage);
       Server_StopMontage(TargetMontage);
    }

    Server_SetAuraActive(false);
    
    if (bIsPillarMode)
    {
       bIsPillarMode = false;
       CurrentTargetPillar = nullptr;
       if (CameraTimelineComp) CameraTimelineComp->Reverse();
    }

    if (bIsBoatMode)
    {
       AMagicBoat* Boat = Cast<AMagicBoat>(CurrentControlledActor);
       if (Boat)
       {
          GetCharacterMovement()->bEnablePhysicsInteraction = true;
          Server_SetBoatRideState(Boat, false);

          if (IsLocallyControlled())
          {
             if (TopDownCamera) TopDownCamera->SetActive(false);
             if (Camera) Camera->SetActive(true);
          }
       }

       if (CurrentControlledActor)
       {
          Server_InteractActor(CurrentControlledActor, FVector::ZeroVector);
       }

       bIsBoatMode = false;
       CurrentControlledActor = nullptr;
    }

    if (SpringArm)
    {
       SpringArm->bUsePawnControlRotation = true;
       SpringArm->bInheritPitch = true;
       SpringArm->bInheritYaw = true;
       SpringArm->bInheritRoll = true;
    }
}

// ====================================================================================
// [Section 7] 네트워크 RPC 및 이펙트 (RPC & VFX)
// ====================================================================================

void AMageCharacter::Server_ProcessSkill_Implementation(FName SkillRowName, FVector TargetLocation)
{
    ProcessSkill(SkillRowName, TargetLocation);
}

void AMageCharacter::Server_PlayMontage_Implementation(UAnimMontage* MontageToPlay)
{
    Multicast_PlayMontage(MontageToPlay);
}

void AMageCharacter::Multicast_PlayMontage_Implementation(UAnimMontage* MontageToPlay)
{
    if (MontageToPlay && !IsLocallyControlled()) PlayAnimMontage(MontageToPlay);
}

void AMageCharacter::Server_StopMontage_Implementation(UAnimMontage* MontageToStop)
{
    Multicast_StopMontage(MontageToStop);
}

void AMageCharacter::Multicast_StopMontage_Implementation(UAnimMontage* MontageToStop)
{
    if (MontageToStop && !IsLocallyControlled()) StopAnimMontage(MontageToStop);
}

void AMageCharacter::Server_InteractActor_Implementation(AActor* TargetActor, FVector Direction)
{
    if (TargetActor)
    {
       if (IMagicInteractableInterface* Interface = Cast<IMagicInteractableInterface>(TargetActor))
          Interface->ProcessMageInput(Direction);
    }
}

void AMageCharacter::Server_TriggerPillarFall_Implementation(APillar* TargetPillar)
{
    if (TargetPillar) TargetPillar->TriggerFall();
}

void AMageCharacter::Server_SetBoatRideState_Implementation(AMagicBoat* Boat, bool bRiding)
{
    if (Boat) Boat->SetRideState(bRiding);
}

void AMageCharacter::Multicast_SetTrailActive_Implementation(bool bActive)
{
    if (StaffTrailComp)
    {
       if (bActive) StaffTrailComp->Activate(true);
       else StaffTrailComp->Deactivate();
    }
}

void AMageCharacter::Server_SetAuraActive_Implementation(bool bActive)
{
    Multicast_SetAuraActive(bActive);
}

void AMageCharacter::Multicast_SetAuraActive_Implementation(bool bActive)
{
    if (JobAuraComp)
    {
       if (bActive) JobAuraComp->Activate(true);
       else JobAuraComp->Deactivate();
    }
}

void AMageCharacter::Multicast_PlaySkill1VFX_Implementation()
{
    if (Skill1CastEffect)
    {
       USceneComponent* AttachTarget = CachedWeaponMesh ? CachedWeaponMesh : Cast<USceneComponent>(GetMesh());
       FName SocketName = TEXT("Weapon_Root_R"); 

       UGameplayStatics::SpawnEmitterAttached(
          Skill1CastEffect,
          AttachTarget,
          SocketName,
          FVector::ZeroVector,
          FRotator::ZeroRotator,
          EAttachLocation::SnapToTarget,
          true 
       );
    }
}

void AMageCharacter::Multicast_PlaySkill2VFX_Implementation()
{
   if (Skill2CastEffect)
   {
      USceneComponent* AttachTarget = CachedWeaponMesh ? (USceneComponent*)CachedWeaponMesh : Cast<USceneComponent>(GetMesh());
      FName SocketName = TEXT("Weapon_Root_R"); 

      Skill2CastComp = UGameplayStatics::SpawnEmitterAttached(
         Skill2CastEffect,
         AttachTarget,
         SocketName,  
         FVector::ZeroVector,
         FRotator::ZeroRotator,
         EAttachLocation::SnapToTarget,
         true
      );
   }
}

void AMageCharacter::Multicast_StopSkill2VFX_Implementation()
{
   if (IsValid(Skill2CastComp))
   {
      Skill2CastComp->DeactivateSystem(); 
      Skill2CastComp = nullptr;
   }
}

float AMageCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    if (ActualDamage > 0.0f && (bIsPillarMode || bIsBoatMode))
    {
       EndJobAbility();
    }

    return ActualDamage;
}