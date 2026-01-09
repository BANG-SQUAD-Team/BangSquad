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

AMageCharacter::AMageCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    GetCharacterMovement()->MaxWalkSpeed = 500.f;
    JumpCooldownTimer = 1.0f;
    UnlockedStageLevel = 1;

    FocusedPillar = nullptr;
    CurrentTargetPillar = nullptr;

    // 타임라인 컴포넌트 생성
    CameraTimelineComp = CreateDefaultSubobject<UTimelineComponent>(TEXT("CameraTimelineComp"));
    
    if (SpringArm) 
    {
        // 피벗을 머리 위로 올려서 시야 확보
        SpringArm->TargetOffset = FVector(0.0f, 0.0f, 150.0f); 
        SpringArm->ProbeSize = 12.0f; 
    }
}

void AMageCharacter::BeginPlay()
{
    Super::BeginPlay();

    // 초기값 저장 (나중에 복구용)
    if (SpringArm)
    {
        DefaultArmLength = SpringArm->TargetArmLength;
        DefaultSocketOffset = SpringArm->SocketOffset;
    }

    // 타임라인 바인딩
    FOnTimelineFloat TimelineProgress; 
    TimelineProgress.BindDynamic(this, &AMageCharacter::CameraTimelineProgress);
    
    if (CameraCurve)
    {
        CameraTimelineComp->AddInterpFloat(CameraCurve, TimelineProgress);
    }
    else
    {
        // 커브 없으면 기본 선형 보간 (0~1초)
        CameraTimelineComp->AddInterpFloat(nullptr, TimelineProgress);
        CameraTimelineComp->SetTimelineLength(1.0f); 
    }

    // 종료 이벤트 바인딩
    FOnTimelineEvent TimelineFinishedEvent;
    TimelineFinishedEvent.BindDynamic(this, &AMageCharacter::OnCameraTimelineFinished);
    CameraTimelineComp->SetTimelineFinishedFunc(TimelineFinishedEvent);

    CameraTimelineComp->SetLooping(false);
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

    // 타임라인 업데이트 (필수)
    if (CameraTimelineComp)
    {
        CameraTimelineComp->TickComponent(DeltaTime, ELevelTick::LEVELTICK_TimeOnly, nullptr);
    }

    // [로직 1] 상태 탈출: 기둥이 없거나 쓰러졌으면 강제 종료
    if (bIsJobAbilityActive)
    {
        if (!IsValid(CurrentTargetPillar) || CurrentTargetPillar->bIsFallen)
        {
            EndJobAbility();
            return; 
        }
    }

    // 아웃라인 업데이트
    UpdatePillarInteraction();

    // [로직 2] 락온 및 제스처 (기둥이 멀쩡할 때만)
    if (bIsJobAbilityActive && IsValid(CurrentTargetPillar) && !CurrentTargetPillar->bIsFallen)
    {
        // 1. 시선 고정
        LockOnPillar(DeltaTime);

        // 2. 마우스 제스처
        float MouseX, MouseY;
        APlayerController* PC = Cast<APlayerController>(GetController());
        if (PC)
        {
            PC->GetInputMouseDelta(MouseX, MouseY);
            
            if (FMath::Abs(MouseX) > 0.1f && FMath::Sign(MouseX) == CurrentTargetPillar->RequiredMouseDirection)
            {
                CurrentTargetPillar->TriggerFall();
                // 쓰러지면 다음 프레임에 [로직 1]에 걸려서 자동 종료
            }
        }
    }
}

// 타임라인 진행 (Alpha: 0.0 -> 1.0)
void AMageCharacter::CameraTimelineProgress(float Alpha)
{
    if (!SpringArm) return;

    // 1. 거리: 기본값 <-> 1200 (줌 아웃)
    float NewArmLength = FMath::Lerp(DefaultArmLength, 1200.f, Alpha);
    SpringArm->TargetArmLength = NewArmLength;

    // 2. 오프셋: 기본값 <-> (0,0,0) (중앙 정렬)
    // 오프셋을 0으로 만들어서 화면 정중앙에 기둥을 둠
    FVector NewOffset = FMath::Lerp(DefaultSocketOffset, FVector::ZeroVector, Alpha);
    SpringArm->SocketOffset = NewOffset;

    // 회전은 LockOnPillar에서 담당하므로 여기선 건드리지 않음
}

void AMageCharacter::JobAbility()
{
    if (FocusedPillar)
    {
        CurrentTargetPillar = FocusedPillar;
        bIsJobAbilityActive = true;

        if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            // 마우스 직접 조작 막기
            PC->SetIgnoreLookInput(true);
        }

        if (SpringArm) 
        {
            // 스프링암이 컨트롤러 회전(LockOnPillar 계산값)을 따르도록 설정
            SpringArm->bUsePawnControlRotation = true; 
            SpringArm->bInheritPitch = true; 
            SpringArm->bInheritYaw = true;
            SpringArm->bInheritRoll = true;
        }

        // 줌 아웃 시작
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

    // 조작 권한 즉시 복구
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

    // 줌 되감기 (원래 위치로)
    if (CameraTimelineComp) 
    {
        CameraTimelineComp->Reverse();
    }
}

void AMageCharacter::OnCameraTimelineFinished()
{
    // 완전히 되감아졌을 때(0.0)만 실행
    if (CameraTimelineComp && CameraTimelineComp->GetPlaybackPosition() <= 0.01f)
    {
        APlayerController* PC = Cast<APlayerController>(GetController());
        if (PC)
        {
            // 마우스/인풋 모드 정리
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
            HitPillar->PillarMesh->SetCustomDepthStencilValue(250); // 아웃라인 색상값 (필수)

            FocusedPillar = HitPillar;
        }
    }
    else if (FocusedPillar)
    {
        FocusedPillar->PillarMesh->SetRenderCustomDepth(false);
        FocusedPillar = nullptr;
    }
}

void AMageCharacter::LockOnPillar(float DeltaTime)
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    
    if (PC && CurrentTargetPillar && !CurrentTargetPillar->bIsFallen)
    {
        // 1. 시작점: 현재 카메라 위치
        FVector StartLoc = Camera->GetComponentLocation();
        
        // 2. 목표점: 기둥 상단부
        FVector TargetLoc = CurrentTargetPillar->GetActorLocation() + FVector(0.0f, 0.0f, 300.0f);

        // 3. 회전 계산 (LookAt)
        FRotator TargetRot = UKismetMathLibrary::FindLookAtRotation(StartLoc, TargetLoc);

        // 4. 부드럽게 회전
        FRotator NewRot = FMath::RInterpTo(PC->GetControlRotation(), TargetRot, DeltaTime, 5.0f);
        
        PC->SetControlRotation(NewRot);
    }
}

// 스킬 관련 로직 (유지)
void AMageCharacter::ProcessSkill(FName SkillRowName)
{
    if (!SkillDataTable) return;
    static const FString ContextString(TEXT("SkillContext"));
    FSkillData* Data = SkillDataTable->FindRow<FSkillData>(SkillRowName, ContextString);
    if (Data)
    {
        if (!IsSkillUnlocked(Data->RequiredStage)) return;
        if (Data->SkillMontage) PlayAnimMontage(Data->SkillMontage);
        if (Data->ProjectileClass)
        {
            FVector SpawnLoc = GetActorLocation() + GetActorForwardVector() * 100.f;
            FRotator SpawnRot = GetControlRotation();
            GetWorld()->SpawnActor<AMageProjectile>(Data->ProjectileClass, SpawnLoc, SpawnRot);
        }
    }
}

void AMageCharacter::Attack() { ProcessSkill(TEXT("Attack")); }
void AMageCharacter::Skill1() { ProcessSkill(TEXT("Skill1")); }
void AMageCharacter::Skill2() { ProcessSkill(TEXT("Skill2")); }