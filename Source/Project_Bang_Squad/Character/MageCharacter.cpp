#include "Project_Bang_Squad/Character/MageCharacter.h"
#include "Project_Bang_Squad/Projectile/MageProjectile.h"
#include "Project_Bang_Squad/Character/Pillar.h" // [유지] 기둥 헤더
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h" 
#include "EnhancedInputComponent.h"
#include "Kismet/KismetMathLibrary.h" 
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/DataTable.h"
#include "Components/TimelineComponent.h" 
#include "Curves/CurveFloat.h" 
#include "Net/UnrealNetwork.h"

AMageCharacter::AMageCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    bUseControllerRotationYaw = true;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;
    
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
    
    AttackCooldownTime = 1.f;
    UnlockedStageLevel = 1;

    FocusedPillar = nullptr;
    CurrentTargetPillar = nullptr;

    // [기둥용] 타임라인 컴포넌트
    CameraTimelineComp = CreateDefaultSubobject<UTimelineComponent>(TEXT("CameraTimelineComp"));
    
    // [보트용] 탑다운 카메라 컴포넌트
    TopDownSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("TopDownSpringArm"));
    TopDownSpringArm->SetupAttachment(GetRootComponent());
    TopDownSpringArm->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f)); // 수직
    TopDownSpringArm->TargetArmLength = 1200.0f; 
    TopDownSpringArm->bDoCollisionTest = false; 

    TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
    TopDownCamera->SetupAttachment(TopDownSpringArm, USpringArmComponent::SocketName);
    TopDownCamera->SetAutoActivate(false); 
    
    if (SpringArm) 
    {
        SpringArm->TargetOffset = FVector(0.0f, 0.0f, 150.0f); 
        SpringArm->ProbeSize = 12.0f; 
        SpringArm->bUsePawnControlRotation = true;
    }
}

void AMageCharacter::BeginPlay()
{
    Super::BeginPlay();

    // [강제 적용] 블루프린트 설정 무시하고 코드로 강제 설정
    // 이 코드가 없으면 BP가 생성자 값을 씹어버립니다.
    if (SpringArm)
    {
        SpringArm->bUsePawnControlRotation = true; // 마우스 회전 따라가기 (필수)
        SpringArm->bInheritPitch = true;           // 위아래 회전 허용
        SpringArm->bInheritYaw = true;             // 좌우 회전 허용
        SpringArm->bInheritRoll = false;           
        
        // 기존 암 길이/오프셋 저장
        DefaultArmLength = SpringArm->TargetArmLength;
        DefaultSocketOffset = SpringArm->SocketOffset;
    }
    
    // 시작 시 탑다운 카메라는 꺼둠
    if (TopDownCamera) TopDownCamera->SetActive(false);
    
    // 타임라인 설정
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
}

void AMageCharacter::OnDeath()
{
    if (bIsDead) return;
    
    // 직업 능력 강제 종료
    if (bIsPillarMode || bIsBoatMode)
    {
        EndJobAbility();
    }
    
    GetWorldTimerManager().ClearTimer(ComboResetTimer);
    GetWorldTimerManager().ClearTimer(ProjectileTimerHandle);
    StopAnimMontage();
    
    Super::OnDeath();
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

void AMageCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (CameraTimelineComp)
    {
        CameraTimelineComp->TickComponent(DeltaTime, ELevelTick::LEVELTICK_TimeOnly, nullptr);
    }

    // 탈출 조건 (유지)
    if (bIsPillarMode)
    {
        if (!IsValid(CurrentTargetPillar) || CurrentTargetPillar->bIsFallen)
        {
            EndJobAbility(); 
            return; 
        }
    }

    CheckInteractableTarget(); 

    // [수정] 락온만 남기고 마우스 감지 코드는 싹 지움 (Look으로 이사감)
    if (bIsPillarMode && IsValid(CurrentTargetPillar) && !CurrentTargetPillar->bIsFallen)
    {
        LockOnPillar(DeltaTime);
        
        // [삭제됨] 여기서 MouseX 체크하던 코드 제거
    }
}

void AMageCharacter::CheckInteractableTarget()
{
    // 조종 중엔 타겟 변경 금지
    if (bIsPillarMode || bIsBoatMode) return;

    FHitResult HitResult;
    FVector Start = Camera->GetComponentLocation();
    FVector End = Start + (Camera->GetForwardVector() * TraceDistance);
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this); 

    bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params);
    AActor* HitActor = HitResult.GetActor();

    // 1. 기둥(Pillar)인지 확인
    APillar* HitPillar = Cast<APillar>(HitActor);
    
    // 2. 인터페이스(Boat 등)인지 확인
    // Implements는 "이 인터페이스를 가지고 있니?"라고 묻는 것
    bool bIsInterface = (HitActor && HitActor->Implements<UMagicInteractableInterface>());

    // A. 기둥 처리 (기존 로직)
    if (HitPillar && !HitPillar->bIsFallen)
    {
        if (FocusedPillar != HitPillar)
        {
            // 이전 하이라이트 끄기
            if (FocusedPillar) FocusedPillar->PillarMesh->SetRenderCustomDepth(false);
            
            // [수정] 인터페이스 끄기 (Cast 사용)
            if (HoveredActor)
            {
                if (IMagicInteractableInterface* Interface = Cast<IMagicInteractableInterface>(HoveredActor))
                {
                    Interface->SetMageHighlight(false);
                }
            }
            HoveredActor = nullptr;

            // 새 기둥 켜기
            HitPillar->PillarMesh->SetRenderCustomDepth(true);
            HitPillar->PillarMesh->SetCustomDepthStencilValue(250);
            FocusedPillar = HitPillar;
        }
    }
    // B. 인터페이스 액터 처리 (신규 로직)
    else if (bIsInterface)
    {
        if (HoveredActor != HitActor)
        {
            // 이전 하이라이트 끄기
            if (FocusedPillar) 
            {
                FocusedPillar->PillarMesh->SetRenderCustomDepth(false);
                FocusedPillar = nullptr;
            }
            // [수정] 이전 인터페이스 끄기
            if (HoveredActor)
            {
                if (IMagicInteractableInterface* Interface = Cast<IMagicInteractableInterface>(HoveredActor))
                {
                    Interface->SetMageHighlight(false);
                }
            }

            // [수정] 새 액터 켜기 (Cast 사용)
            if (IMagicInteractableInterface* Interface = Cast<IMagicInteractableInterface>(HitActor))
            {
                Interface->SetMageHighlight(true);
            }
            HoveredActor = HitActor;
        }
    }
    // C. 허공
    else
    {
        if (FocusedPillar)
        {
            FocusedPillar->PillarMesh->SetRenderCustomDepth(false);
            FocusedPillar = nullptr;
        }
        //  인터페이스 끄기
        if (HoveredActor)
        {
            if (IMagicInteractableInterface* Interface = Cast<IMagicInteractableInterface>(HoveredActor))
            {
                Interface->SetMageHighlight(false);
            }
        }
        HoveredActor = nullptr;
    }
}

// =========================================================
//  직업 능력 (우클릭) 시작/종료
// =========================================================
void AMageCharacter::JobAbility()
{
    if (bIsDead) return;
    
    // Case 1: 기둥 모드 진입
    if (FocusedPillar)
    {
        bIsPillarMode = true;
        CurrentTargetPillar = FocusedPillar;

        // [삭제] 이 코드가 마우스 입력을 0으로 만들어서 문제였음!
        // if (APlayerController* PC = Cast<APlayerController>(GetController()))
        // {
        //     PC->SetIgnoreLookInput(true); 
        // }

        if (SpringArm) 
        {
            SpringArm->bUsePawnControlRotation = true; 
            SpringArm->bInheritPitch = true; 
            SpringArm->bInheritYaw = true;
            SpringArm->bInheritRoll = true;
        }
        if (CameraTimelineComp) CameraTimelineComp->Play();
    }
    // Case 2: 보트 모드 (그대로 유지)
    else if (HoveredActor)
    {
        bIsBoatMode = true;
        CurrentControlledActor = HoveredActor;

        if (IsLocallyControlled())
        {
            if (Camera) Camera->SetActive(false);
            if (TopDownCamera) TopDownCamera->SetActive(true);
        }
    }
}

void AMageCharacter::EndJobAbility()
{
    // Case 1: 기둥 모드 종료
    if (bIsPillarMode)
    {
        bIsPillarMode = false;
        CurrentTargetPillar = nullptr;

        // [삭제] 위에서 안 잠갔으니 풀 필요도 없음
        // if (APlayerController* PC = Cast<APlayerController>(GetController()))
        // {
        //     PC->SetIgnoreLookInput(false);
        // }
        
        if (CameraTimelineComp) CameraTimelineComp->Reverse();
    }

    // Case 2: 보트 모드 종료 (그대로 유지)
    if (bIsBoatMode)
    {
        if (CurrentControlledActor)
        {
            Server_InteractActor(CurrentControlledActor, FVector::ZeroVector);
        }
        bIsBoatMode = false;
        CurrentControlledActor = nullptr;

        if (IsLocallyControlled())
        {
            if (TopDownCamera) TopDownCamera->SetActive(false);
            if (Camera) Camera->SetActive(true);
        }
    }
    
    // 공통 복구
    if (SpringArm)
    {
        SpringArm->bUsePawnControlRotation = true;
        SpringArm->bInheritPitch = true; 
        SpringArm->bInheritYaw = true;
        SpringArm->bInheritRoll = true;
    }
}

void AMageCharacter::Look(const FInputActionValue& Value)
{
    FVector2D LookInput = Value.Get<FVector2D>();

    // 1. 보트 모드
    if (bIsBoatMode && CurrentControlledActor)
    {
        if (TopDownCamera)
        {
            FVector ScreenUp = TopDownCamera->GetUpVector();
            FVector ScreenRight = TopDownCamera->GetRightVector();
            ScreenUp.Z = 0; ScreenRight.Z = 0;
            ScreenUp.Normalize(); ScreenRight.Normalize();

            FVector MoveDir = (ScreenUp * LookInput.Y) + (ScreenRight * LookInput.X);

            if (!MoveDir.IsNearlyZero())
            {
                Server_InteractActor(CurrentControlledActor, MoveDir);
            }
        }
    }
    // 2. [수정] 기둥 모드: 시점 회전은 막고, 제스처만 감지!
    else if (bIsPillarMode)
    {
        // AddControllerYawInput을 호출 안 하니까 시점은 자동으로 잠김.
        // 대신 여기서 마우스 흔들기를 직접 체크함.
        if (IsValid(CurrentTargetPillar) && !CurrentTargetPillar->bIsFallen)
        {
            // LookInput.X가 마우스 좌우 움직임임
            // 감도 조절: 0.5f 정도로 설정 (너무 민감하면 올리세요)
            if (FMath::Abs(LookInput.X) > 0.5f && FMath::Sign(LookInput.X) == CurrentTargetPillar->RequiredMouseDirection)
            {
                // 1. 무너뜨리기
                Server_TriggerPillarFall(CurrentTargetPillar);
                
                // 2. 즉시 탈출 (락온 해제)
                EndJobAbility();
            }
        }
    }
    // 3. 일반 모드
    else
    {
        Super::Look(Value);
    }
}

// =========================================================
//  공격 및 스킬 로직 (기존 유지)
// =========================================================
void AMageCharacter::Attack()
{
    if (!CanAttack()) return;
    StartAttackCooldown();
    
    FName SkillName = (CurrentComboIndex == 0) ? TEXT("Attack_A") : TEXT("Attack_B");
    ProcessSkill(SkillName);

    CurrentComboIndex++;
    if (CurrentComboIndex > 1) CurrentComboIndex = 0;

    GetWorldTimerManager().ClearTimer(ComboResetTimer);
    GetWorldTimerManager().SetTimer(ComboResetTimer, this, &AMageCharacter::ResetCombo, 1.2f, false);
}

void AMageCharacter::ResetCombo() { CurrentComboIndex = 0; }
void AMageCharacter::Skill1() { if (!bIsDead) ProcessSkill(TEXT("Skill1")); }
void AMageCharacter::Skill2() { if (!bIsDead) ProcessSkill(TEXT("Skill2")); }

void AMageCharacter::ProcessSkill(FName SkillRowName)
{
    if (!SkillDataTable) return;
    static const FString ContextString(TEXT("SkillContext"));
    FSkillData* Data = SkillDataTable->FindRow<FSkillData>(SkillRowName, ContextString);
    
    if (Data)
    {
        if (!IsSkillUnlocked(Data->RequiredStage)) return;

        if (Data->SkillMontage) PlayAnimMontage(Data->SkillMontage);

        if (HasAuthority())
        {
            if (Data->SkillMontage) Server_PlayMontage(Data->SkillMontage);

            if (Data->ProjectileClass)
            {
                FTimerDelegate TimerDel;
                TimerDel.BindUObject(this, &AMageCharacter::SpawnDelayedProjectile, Data->ProjectileClass.Get());
                
                if (Data->ActionDelay > 0.0f)
                    GetWorldTimerManager().SetTimer(ProjectileTimerHandle, TimerDel, Data->ActionDelay, false);
                else
                    SpawnDelayedProjectile(Data->ProjectileClass);
            }
        }
        else
        {
            Server_ProcessSkill(SkillRowName);
        }
    }
}

// =========================================================
//  RPC 구현부
// =========================================================
void AMageCharacter::Server_ProcessSkill_Implementation(FName SkillRowName)
{
    ProcessSkill(SkillRowName);
}

void AMageCharacter::SpawnDelayedProjectile(UClass* ProjectileClass)
{
    if (!HasAuthority() || !ProjectileClass) return;

    FVector SpawnLoc;
    FRotator SpawnRot = GetControlRotation(); 
    FName SocketName = TEXT("Weapon_Root_R"); 

    if (GetMesh() && GetMesh()->DoesSocketExist(SocketName))
        SpawnLoc = GetMesh()->GetSocketLocation(SocketName);
    else
        SpawnLoc = GetActorLocation() + (GetActorForwardVector() * 100.f);

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;            
    SpawnParams.Instigator = GetInstigator(); 

    GetWorld()->SpawnActor<AActor>(ProjectileClass, SpawnLoc, SpawnRot, SpawnParams);
}

void AMageCharacter::Server_PlayMontage_Implementation(UAnimMontage* MontageToPlay)
{
    Multicast_PlayMontage(MontageToPlay);
}

void AMageCharacter::Multicast_PlayMontage_Implementation(UAnimMontage* MontageToPlay)
{
    if (MontageToPlay && !IsLocallyControlled())
    {
        PlayAnimMontage(MontageToPlay);
    }
}


void AMageCharacter::Server_InteractActor_Implementation(AActor* TargetActor, FVector Direction)
{
    // [수정] Cast를 이용해서 안전하게 호출
    if (TargetActor)
    {
        // 인터페이스로 형변환 시도
        IMagicInteractableInterface* Interface = Cast<IMagicInteractableInterface>(TargetActor);
        
        // 형변환 성공하면(인터페이스가 있으면) 함수 실행
        if (Interface)
        {
            Interface->ProcessMageInput(Direction);
        }
    }
}

// [유지] 기둥 파괴
void AMageCharacter::Server_TriggerPillarFall_Implementation(APillar* TargetPillar)
{
    if (TargetPillar) TargetPillar->TriggerFall();
}

// =========================================================
//  타임라인 & 락온 (기존 유지)
// =========================================================
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