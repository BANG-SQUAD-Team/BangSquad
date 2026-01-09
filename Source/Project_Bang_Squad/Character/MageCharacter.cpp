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
#include "Net/UnrealNetwork.h" // 네트워크 헤더

AMageCharacter::AMageCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    GetCharacterMovement()->MaxWalkSpeed = 500.f;
    JumpCooldownTimer = 1.0f;
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

    // [로직 1] 상태 탈출
    if (bIsJobAbilityActive)
    {
        if (!IsValid(CurrentTargetPillar) || CurrentTargetPillar->bIsFallen)
        {
            EndJobAbility();
            return; 
        }
    }

    UpdatePillarInteraction();

    // [로직 2] 락온 및 제스처
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
                // [변경됨] 클라이언트가 직접 넘기지 않고, 서버에게 요청함
                Server_TriggerPillarFall(CurrentTargetPillar);
                
                // (선택 사항) 반응성을 위해 내 화면에서는 미리 종료 처리를 시작할 수도 있지만,
                // 안전하게 서버가 처리해서 bIsFallen이 바뀌면 다음 틱에 [로직 1]에서 자동 종료되게 둠.
            }
        }
    }
}

// [추가됨] 서버에서 실행되는 함수 (RPC Implementation)
void AMageCharacter::Server_TriggerPillarFall_Implementation(APillar* TargetPillar)
{
    if (TargetPillar)
    {
        // 서버에서 기둥을 넘어뜨림 -> Pillar의 Replicated 변수 변경 -> 모든 클라이언트에 전파됨
        TargetPillar->TriggerFall();
    }
}

void AMageCharacter::CameraTimelineProgress(float Alpha)
{
    if (!SpringArm) return;

    float NewArmLength = FMath::Lerp(DefaultArmLength, 1200.f, Alpha);
    SpringArm->TargetArmLength = NewArmLength;

    FVector NewOffset = FMath::Lerp(DefaultSocketOffset, FVector::ZeroVector, Alpha);
    SpringArm->SocketOffset = NewOffset;
}

void AMageCharacter::JobAbility()
{
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

void AMageCharacter::ProcessSkill(FName SkillRowName)
{
    if (!SkillDataTable) return;
    static const FString ContextString(TEXT("SkillContext"));
    FSkillData* Data = SkillDataTable->FindRow<FSkillData>(SkillRowName, ContextString);
    
    if (Data)
    {
        if (!IsSkillUnlocked(Data->RequiredStage)) return;

        // 1. 애니메이션 재생 (로컬)
        if (Data->SkillMontage) 
        {
            PlayAnimMontage(Data->SkillMontage);
        }

        // 2. 투사체 소환 로직
        if (Data->ProjectileClass)
        {
            FVector SpawnLoc;
            FRotator SpawnRot = GetControlRotation(); // 조준 방향 (카메라가 보는 방향)

            //  소켓 이름 (에디터에 있는 소켓 이름과 똑같아야 함!)
            FName SocketName = TEXT("Weapon_Root_R"); 

            // 소켓이 있으면 소켓 위치 사용, 없으면 그냥 몸 앞에서 발사
            if (GetMesh() && GetMesh()->DoesSocketExist(SocketName))
            {
                SpawnLoc = GetMesh()->GetSocketLocation(SocketName);
            }
            else
            {
                // 소켓 못 찾았을 때 비상용 
                SpawnLoc = GetActorLocation() + (GetActorForwardVector() * 100.f);
            }

            //  서버에게 생성을 요청함
            Server_SpawnProjectile(Data->ProjectileClass, SpawnLoc, SpawnRot);
        }
    }
}

// 서버에서 실제 투사체를 만드는 곳
void AMageCharacter::Server_SpawnProjectile_Implementation(UClass* ProjectileClassToSpawn, FVector Location, FRotator Rotation)
{
    if (!ProjectileClassToSpawn) return;

    // 1. 스폰 파라미터 설정 (아까 배운 핵심 내용!)
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;            // 주인은 나! (MageCharacter)
    SpawnParams.Instigator = GetInstigator(); 

    // 2. 실제 소환 (서버에서만 일어남 -> 모든 클라이언트로 복제됨)
    GetWorld()->SpawnActor<AActor>(
        ProjectileClassToSpawn,
        Location,
        Rotation,
        SpawnParams
    );
}


void AMageCharacter::Attack() { ProcessSkill(TEXT("Attack")); }
void AMageCharacter::Skill1() { ProcessSkill(TEXT("Skill1")); }
void AMageCharacter::Skill2() { ProcessSkill(TEXT("Skill2")); }