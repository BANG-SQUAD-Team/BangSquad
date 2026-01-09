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
#include "Components/TimelineComponent.h" // [필수] 타임라인 헤더
#include "Curves/CurveFloat.h" // [필수] 커브 헤더

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
}

void AMageCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (SpringArm)
    {
        DefaultArmLength = SpringArm->TargetArmLength;
        DefaultSocketOffset = SpringArm->SocketOffset;
    }

    if (CameraCurve)
    {
        // 1. 진행 함수 바인딩 
       
        FOnTimelineFloat TimelineProgress; 
        TimelineProgress.BindDynamic(this, &AMageCharacter::CameraTimelineProgress);
        
        CameraTimelineComp->AddInterpFloat(CameraCurve, TimelineProgress);

        // 2. 종료 함수 바인딩
        FOnTimelineEvent TimelineFinishedEvent;
        TimelineFinishedEvent.BindDynamic(this, &AMageCharacter::OnCameraTimelineFinished);
        CameraTimelineComp->SetTimelineFinishedFunc(TimelineFinishedEvent);

        CameraTimelineComp->SetLooping(false);
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

void AMageCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // [안전장치] 우클릭 떼면 즉시 종료
    if (bIsJobAbilityActive)
    {
        APlayerController* PC = Cast<APlayerController>(GetController());
        if (PC && !PC->IsInputKeyDown(EKeys::RightMouseButton))
        {
            EndJobAbility(); 
        }
    }

    UpdatePillarInteraction();

    // [수정된 부분] 
    // IsValid 체크 + "기둥이 쓰러지지 않았을 때만(!bIsFallen)" 락온을 실행합니다.
    // 기둥이 쓰러지면 즉시 시점 고정이 풀려서 마우스로 돌릴 수 있게 됩니다.
    if (bIsJobAbilityActive && IsValid(CurrentTargetPillar) && !CurrentTargetPillar->bIsFallen)
    {
        // 1. 시선 고정 (쓰러지기 전까지만 작동)
        LockOnPillar(DeltaTime);

        // 2. 제스처 입력 처리
        float MouseX, MouseY;
        APlayerController* PC = Cast<APlayerController>(GetController());
        if (PC)
        {
            PC->GetInputMouseDelta(MouseX, MouseY);
            if (FMath::Abs(MouseX) > 0.1f && FMath::Sign(MouseX) == CurrentTargetPillar->RequiredMouseDirection)
            {
                CurrentTargetPillar->TriggerFall();
            }
        }
    }
}

// 타임라인이 매 프레임 호출하는 함수 (Alpha: 0~1)
void AMageCharacter::CameraTimelineProgress(float Alpha)
{
    if (!SpringArm) return;

    // 1. 거리: 기본값 <-> 1200 사이 보간 (줌 아웃) - 이건 유지
    float NewArmLength = FMath::Lerp(DefaultArmLength, 1200.f, Alpha);
    SpringArm->TargetArmLength = NewArmLength;

    // 2. 오프셋: 기본값 <-> (0,0,0) 사이 보간 - 이건 유지
    FVector NewOffset = FMath::Lerp(DefaultSocketOffset, FVector::ZeroVector, Alpha);
    SpringArm->SocketOffset = NewOffset;

    // [삭제됨] 회전(Rotation) 관련 코드는 여기서 모두 지웁니다!
    // 이유: LockOnPillar 함수가 회전을 담당해야 하기 때문입니다.
    // 여기서 회전을 건드리면 락온이랑 싸워서 시점이 고정돼버립니다.
}

void AMageCharacter::JobAbility()
{
    if (FocusedPillar)
    {
        CurrentTargetPillar = FocusedPillar;
        bIsJobAbilityActive = true;

        if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            // 마우스 직접 조작은 막음 (코드로만 회전시킬 거니까)
            PC->SetIgnoreLookInput(true);
        }

        if (SpringArm) 
        {
            // [중요] 스프링암이 컨트롤러 회전(LockOnPillar가 계산한 값)을 따라가야 함!
            SpringArm->bUsePawnControlRotation = true; 
            
            // Pitch(위아래) 상속을 켜둬야 기둥 위쪽을 쳐다볼 수 있음
            SpringArm->bInheritPitch = true; 
            SpringArm->bInheritYaw = true;
            SpringArm->bInheritRoll = true;
        }

        if (CameraTimelineComp) CameraTimelineComp->Play();
    }
}

void AMageCharacter::EndJobAbility()
{
    // 1. 상태 해제
    bIsJobAbilityActive = false;
    CurrentTargetPillar = nullptr;

    // 2. 마우스 입력 및 스프링암 설정 즉시 복구 (기다리지 않음!)
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->SetIgnoreLookInput(false);
    }

    if (SpringArm)
    {
        SpringArm->bUsePawnControlRotation = true;
        SpringArm->bInheritPitch = true; // [중요] 위아래 시점 즉시 복구
        SpringArm->bInheritYaw = true;
        SpringArm->bInheritRoll = true;
    }

    // 3. 타임라인 되감기 (카메라 줌만 부드럽게 돌아옴)
    if (CameraTimelineComp) 
    {
        CameraTimelineComp->Reverse();
    }
}


void AMageCharacter::OnCameraTimelineFinished()
{
    // 타임라인이 0초(원점)로 돌아왔을 때만 실행
    if (CameraTimelineComp && CameraTimelineComp->GetPlaybackPosition() <= 0.01f)
    {
        APlayerController* PC = Cast<APlayerController>(GetController());
        if (PC)
        {
            // 혹시 모르니 커서 확실히 끄기
            PC->bShowMouseCursor = false;
            PC->bEnableClickEvents = false;
            PC->bEnableMouseOverEvents = false;
            
            // 인풋 모드 확실하게 게임 모드로
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
    
    // 기둥이 존재하고, 아직 쓰러지지 않았을 때만 락온
    if (PC && CurrentTargetPillar && !CurrentTargetPillar->bIsFallen)
    {
        // 1. 내 눈(카메라) 위치
        FVector StartLoc = Camera->GetComponentLocation();
        
        // 2. 목표(기둥) 위치 - 기둥의 중간~상단부를 보게 조정 (Z축 + 300 정도)
        FVector TargetLoc = CurrentTargetPillar->GetActorLocation() + FVector(0.0f, 0.0f, 300.0f);

        // 3. 바라봐야 할 회전값 계산 (FindLookAtRotation 사용이 훨씬 정확함)
        FRotator TargetRot = FRotator(
            UKismetMathLibrary::FindLookAtRotation(StartLoc, TargetLoc)
        );

        // 4. 현재 회전에서 목표 회전으로 부드럽게 이동 (RInterpTo)
        FRotator NewRot = FMath::RInterpTo(PC->GetControlRotation(), TargetRot, DeltaTime, 5.0f);
        
        // 5. 적용
        PC->SetControlRotation(NewRot);
    }
}

// ... (ProcessSkill, Attack, Skill1, Skill2 등은 기존 코드 유지) ...
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