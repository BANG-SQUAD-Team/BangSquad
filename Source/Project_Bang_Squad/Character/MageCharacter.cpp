#include "Project_Bang_Squad/Character/MageCharacter.h"
#include "Project_Bang_Squad/Projectile/MageProjectile.h"
#include "Project_Bang_Squad/Character/Pillar.h" 
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
    
    // 공격 쿨타임
    AttackCooldownTime = 1.f;
    
    
    //해금 레벨
    UnlockedStageLevel = 1;

    FocusedPillar = nullptr;
    CurrentTargetPillar = nullptr;

    CameraTimelineComp = CreateDefaultSubobject<UTimelineComponent>(TEXT("CameraTimelineComp"));
    
    if (SpringArm) 
    {
        SpringArm->TargetOffset = FVector(0.0f, 0.0f, 150.0f); 
        SpringArm->ProbeSize = 12.0f; 
    }
}

void AMageCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (SpringArm)
    {
        DefaultArmLength = SpringArm->TargetArmLength;
        DefaultSocketOffset = SpringArm->SocketOffset;
    }

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
    // 이미 죽었으면 리턴 (중복 방지)
    if (bIsDead) return;
    
    // 마법사 전용 직업능력 (JobAbility) 강제 종료
    if (bIsJobAbilityActive)
    {
        EndJobAbility();
    }
    
    // 공격 관련 타이머들 싹 다 정지
    GetWorldTimerManager().ClearTimer(ComboResetTimer);
    GetWorldTimerManager().ClearTimer(ProjectileTimerHandle);
    
    // 시전 중이던 몽타주 정지
    StopAnimMontage();
    
    // 부모 클래스의 사망 로직 실행
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

    //  상태 탈출
    if (bIsJobAbilityActive)
    {
        if (!IsValid(CurrentTargetPillar) || CurrentTargetPillar->bIsFallen)
        {
            EndJobAbility();
            return; 
        }
    }

    UpdatePillarInteraction();

    //  락온 및 제스처
    if (bIsJobAbilityActive && IsValid(CurrentTargetPillar) && !CurrentTargetPillar->bIsFallen)
    {
        LockOnPillar(DeltaTime);

        float MouseX, MouseY;
        APlayerController* PC = Cast<APlayerController>(GetController());
        if (PC)
        {
            PC->GetInputMouseDelta(MouseX, MouseY);
            
            // 마우스 제스처 감지
            if (FMath::Abs(MouseX) > 0.1f && FMath::Sign(MouseX) == CurrentTargetPillar->RequiredMouseDirection)
            {
                Server_TriggerPillarFall(CurrentTargetPillar);
            }
        }
    }
}

// =========================================================
//  공격 및 콤보 시스템
// =========================================================

void AMageCharacter::Attack()
{
    // 공격 가능한 상태인지 체크, 아니면 그냥 무시
    if (!CanAttack()) return;
    
    // 공격 시작! -> 즉시 쿨타임
    StartAttackCooldown();
    
    // 1. 현재 콤보 순서에 맞는 스킬 이름 선택
    FName SkillName;
    if (CurrentComboIndex == 0)
    {
        SkillName = TEXT("Attack_A"); // 1타
    }
    else
    {
        SkillName = TEXT("Attack_B"); // 2타
    }

    // 2. 스킬 실행
    ProcessSkill(SkillName);

    // 3. 콤보 카운트 증가 (0 -> 1 -> 0 -> 1 ...)
    CurrentComboIndex++;
    if (CurrentComboIndex > 1) 
    {
        CurrentComboIndex = 0;
    }

    // 4. 콤보 리셋 타이머 설정 (1.2초 뒤 초기화)
    GetWorldTimerManager().ClearTimer(ComboResetTimer);
    GetWorldTimerManager().SetTimer(ComboResetTimer, this, &AMageCharacter::ResetCombo, 1.2f, false);
}

void AMageCharacter::ResetCombo()
{
    CurrentComboIndex = 0;
}

void AMageCharacter::Skill1()
{
    if (bIsDead) return;
    ProcessSkill(TEXT("Skill1"));
}
void AMageCharacter::Skill2()
{
    if (bIsDead) return;
    ProcessSkill(TEXT("Skill2"));
}

void AMageCharacter::JobAbility()
{
    if (bIsDead) return;
    
    if (FocusedPillar)
    {
        CurrentTargetPillar = FocusedPillar;
        bIsJobAbilityActive = true;

        if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            PC->SetIgnoreLookInput(true);
        }

        if (SpringArm) 
        {
            SpringArm->bUsePawnControlRotation = true; 
            SpringArm->bInheritPitch = true; 
            SpringArm->bInheritYaw = true;
            SpringArm->bInheritRoll = true;
        }

        if (CameraTimelineComp)
        {
            CameraTimelineComp->Play();
        }
    }
}

void AMageCharacter::EndJobAbility()
{
    bIsJobAbilityActive = false;
    CurrentTargetPillar = nullptr;

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->SetIgnoreLookInput(false);
    }

    if (SpringArm)
    {
        SpringArm->bUsePawnControlRotation = true;
        SpringArm->bInheritPitch = true; 
        SpringArm->bInheritYaw = true;
        SpringArm->bInheritRoll = true;
    }

    if (CameraTimelineComp) 
    {
        CameraTimelineComp->Reverse();
    }
}

void AMageCharacter::ProcessSkill(FName SkillRowName)
{
    if (!SkillDataTable) return;
    static const FString ContextString(TEXT("SkillContext"));
    FSkillData* Data = SkillDataTable->FindRow<FSkillData>(SkillRowName, ContextString);
    
    if (Data)
    {
        if (!IsSkillUnlocked(Data->RequiredStage)) return;

        // 1. 몽타주 재생 (내 화면 + 멀티)
        if (Data->SkillMontage) 
        {
            PlayAnimMontage(Data->SkillMontage);
            Server_PlayMontage(Data->SkillMontage);
        }

        // 2. 투사체 발사 (타이머 적용)
        if (Data->ProjectileClass)
        {
            // 델리게이터로 파라미터(ProjectileClass)를 묶어서 전달
            FTimerDelegate TimerDel;
            TimerDel.BindUObject(this, &AMageCharacter::SpawnDelayedProjectile, Data->ProjectileClass.Get());
            
            if (Data->ActionDelay > 0.0f)
            {
                GetWorldTimerManager().SetTimer(ProjectileTimerHandle, TimerDel, Data->ActionDelay, false);
            }
            else
            {
                // 즉시 발사
                SpawnDelayedProjectile(Data->ProjectileClass);
            }
        }
    }
}

void AMageCharacter::SpawnDelayedProjectile(UClass* ProjectileClass)
{
    if (!ProjectileClass) return;

    // 발사 위치 계산
    FVector SpawnLoc;
    FRotator SpawnRot = GetControlRotation(); 
    FName SocketName = TEXT("Weapon_Root_R"); 

    if (GetMesh() && GetMesh()->DoesSocketExist(SocketName))
    {
        SpawnLoc = GetMesh()->GetSocketLocation(SocketName);
    }
    else
    {
        SpawnLoc = GetActorLocation() + (GetActorForwardVector() * 100.f);
    }

    // 서버에 생성 요청
    Server_SpawnProjectile(ProjectileClass, SpawnLoc, SpawnRot);
}

// =========================================================
// 서버 RPC 구현부
// =========================================================

void AMageCharacter::Server_SpawnProjectile_Implementation(UClass* ProjectileClassToSpawn, FVector Location, FRotator Rotation)
{
    if (!ProjectileClassToSpawn) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;            
    SpawnParams.Instigator = GetInstigator(); 

    GetWorld()->SpawnActor<AActor>(ProjectileClassToSpawn, Location, Rotation, SpawnParams);
}

void AMageCharacter::Server_PlayMontage_Implementation(UAnimMontage* MontageToPlay)
{
    Multicast_PlayMontage(MontageToPlay);
}

void AMageCharacter::Multicast_PlayMontage_Implementation(UAnimMontage* MontageToPlay)
{
    // 내가 아닌 다른 클라이언트에서만 재생
    if (MontageToPlay && !IsLocallyControlled())
    {
        PlayAnimMontage(MontageToPlay);
    }
}

void AMageCharacter::Server_TriggerPillarFall_Implementation(APillar* TargetPillar)
{
    if (TargetPillar)
    {
        TargetPillar->TriggerFall();
    }
}

// =========================================================
// 기타 기능 (타임라인, 상호작용)
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
        APlayerController* PC = Cast<APlayerController>(GetController());
        if (PC)
        {
            PC->bShowMouseCursor = false;
            PC->bEnableClickEvents = false;
            PC->bEnableMouseOverEvents = false;
            
            FInputModeGameOnly InputMode;
            PC->SetInputMode(InputMode);
        }
    }
}

void AMageCharacter::UpdatePillarInteraction()
{
    if (!Camera || bIsJobAbilityActive) return;

    FHitResult HitResult;
    FVector Start = Camera->GetComponentLocation();
    FVector End = Start + (Camera->GetForwardVector() * TraceDistance);

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this); 

    bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params);
    APillar* HitPillar = bHit ? Cast<APillar>(HitResult.GetActor()) : nullptr;

    if (HitPillar && !HitPillar->bIsFallen)
    {
        if (FocusedPillar != HitPillar)
        {
            if (FocusedPillar) FocusedPillar->PillarMesh->SetRenderCustomDepth(false);
            
            HitPillar->PillarMesh->SetRenderCustomDepth(true);
            HitPillar->PillarMesh->SetCustomDepthStencilValue(250);

            FocusedPillar = HitPillar;
        }
    }
    else if (FocusedPillar)
    {
        FocusedPillar->PillarMesh->SetRenderCustomDepth(false);
        FocusedPillar = nullptr;
    }
}

// Pillar에 시점 고정
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