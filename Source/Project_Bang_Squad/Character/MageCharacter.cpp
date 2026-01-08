#include "Project_Bang_Squad/Character/MageCharacter.h"
#include "Project_Bang_Squad/Projectile/MageProjectile.h"
#include "Project_Bang_Squad/Character/Pillar.h" 
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h" 
#include "EnhancedInputComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/DataTable.h"

AMageCharacter::AMageCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    GetCharacterMovement()->MaxWalkSpeed = 500.f;
    JumpCooldownTimer = 1.0f;
    UnlockedStageLevel = 1;

    FocusedPillar = nullptr;
    CurrentTargetPillar = nullptr;
}

void AMageCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (SpringArm)
    {
        // 에디터 컴포넌트 창에서 설정한 값을 미리 저장해둠
        DefaultArmLength = SpringArm->TargetArmLength;
        DefaultSocketOffset = SpringArm->SocketOffset;
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
        }
    }
}

void AMageCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UpdatePillarInteraction(); // 아웃라인 처리

    // 유효성 체크 (IsValid 필수)
    if (bIsJobAbilityActive && IsValid(CurrentTargetPillar))
    {
        // [A] 시선 고정 (마우스 입력이 잠겨서 이제 아주 단단하게 고정됨)
        LockOnPillar(DeltaTime);

        // [B] 카메라 줌 아웃 (기존 로직 유지)
        if (SpringArm)
        {
            SpringArm->bInheritPitch = false;
            FRotator TargetRelRot = FRotator(-15.0f, 0.0f, 0.0f);
            SpringArm->SetRelativeRotation(FMath::RInterpTo(SpringArm->GetRelativeRotation(), TargetRelRot, DeltaTime, 5.0f));
            SpringArm->TargetArmLength = FMath::FInterpTo(SpringArm->TargetArmLength, 1200.f, DeltaTime, 3.0f); // 줌 아웃
            SpringArm->SocketOffset = FMath::VInterpTo(SpringArm->SocketOffset, FVector::ZeroVector, DeltaTime, 3.0f);
        }

        // [C] 제스처 입력 처리
        if (!CurrentTargetPillar->bIsFallen)
        {
            float MouseX, MouseY;
            APlayerController* PC = Cast<APlayerController>(GetController());
            if (PC)
            {
                // IgnoreLookInput이 켜져 있어도, 이 함수는 순수한 마우스 이동량을 가져옵니다.
                PC->GetInputMouseDelta(MouseX, MouseY);

                if (FMath::Abs(MouseX) > 0.1f && FMath::Sign(MouseX) == CurrentTargetPillar->RequiredMouseDirection)
                {
                    CurrentTargetPillar->TriggerFall();
                    // (타이머 없음: 손 뗄 때까지 뷰 유지)
                }
            }
        }
    }
    // 3. 복구 로직
    else if (SpringArm)
    {
        // 복구 시 LookInput이 false로 남아있으면 안 되므로, 혹시 모르니 여기서도 풀어줍니다.
        if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
             // 만약 EndJobAbility가 안 불리고 넘어왔을 경우를 대비한 안전장치
             if (PC->IsLookInputIgnored()) PC->SetIgnoreLookInput(false);
        }

        // ... (기존 카메라 복구 로직) ...
        SpringArm->bInheritPitch = true;
        SpringArm->SetRelativeRotation(FMath::RInterpTo(SpringArm->GetRelativeRotation(), FRotator::ZeroRotator, DeltaTime, 5.0f));
        SpringArm->TargetArmLength = FMath::FInterpTo(SpringArm->TargetArmLength, DefaultArmLength, DeltaTime, 3.0f);
        SpringArm->SocketOffset = FMath::VInterpTo(SpringArm->SocketOffset, DefaultSocketOffset, DeltaTime, 3.0f);
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

    // 상호작용 가능한(아직 안 넘어간) 기둥인지 확인
    if (HitPillar && !HitPillar->bIsFallen)
    {
        if (FocusedPillar != HitPillar)
        {
            if (FocusedPillar) FocusedPillar->PillarMesh->SetRenderCustomDepth(false);
            
            HitPillar->PillarMesh->SetRenderCustomDepth(true);
            FocusedPillar = HitPillar;
            
            // TODO: UI에 HitPillar->InteractionText 표시
        }
    }
    else if (FocusedPillar)
    {
        FocusedPillar->PillarMesh->SetRenderCustomDepth(false);
        FocusedPillar = nullptr;
        // TODO: UI 숨기기
    }
}

void AMageCharacter::LockOnPillar(float DeltaTime)
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (PC && CurrentTargetPillar)
    {
        // 기둥의 중심보다 500 unit 위(기둥 머리 꼭대기)를 바라봄
        // 이렇게 해야 카메라가 아래에서 위를 쳐다보는 앵글이 나옵니다.
        FVector TargetLoc = CurrentTargetPillar->GetActorLocation() + FVector(0, 0, 500.f);
        
        FVector Dir = (TargetLoc - Camera->GetComponentLocation()).GetSafeNormal();
        FRotator TargetRot = Dir.Rotation();

        // 고정 속도 조금 더 빠르게 (8.0f)
        FRotator NewRot = FMath::RInterpTo(PC->GetControlRotation(), TargetRot, DeltaTime, 8.0f);
        PC->SetControlRotation(NewRot);
    }
}

void AMageCharacter::JobAbility()
{
    // 1. 타겟이 있을 때만 로직 실행
    if (FocusedPillar)
    {
        // ... (기존 스킬 데이터 처리 로직 등) ...

        CurrentTargetPillar = FocusedPillar;
        bIsJobAbilityActive = true;

        // [핵심 추가] 카메라 회전 잠금!
        // 이제 마우스를 움직여도 시선이 돌아가지 않습니다.
        if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            PC->SetIgnoreLookInput(true);
        }
    }
}

void AMageCharacter::EndJobAbility()
{
    bIsJobAbilityActive = false;
    CurrentTargetPillar = nullptr;

    // [핵심 추가] 카메라 회전 잠금 해제!
    // 다시 마우스로 화면을 돌릴 수 있게 됩니다.
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->SetIgnoreLookInput(false);
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
        if (Data->SkillMontage) PlayAnimMontage(Data->SkillMontage);

        if (Data->ProjectileClass)
        {
            FVector SpawnLocation = GetActorLocation() + GetActorForwardVector() * 100.0f;
            FRotator SpawnRotation = GetControlRotation();
            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = this;
            SpawnParams.Instigator = GetInstigator();

            if (AMageProjectile* Projectile = GetWorld()->SpawnActor<AMageProjectile>(Data->ProjectileClass, SpawnLocation, SpawnRotation, SpawnParams))
            {
                Projectile->Damage = Data->Damage;
            }
        }
    }
}

void AMageCharacter::Attack() { ProcessSkill(TEXT("Attack")); }
void AMageCharacter::Skill1() { ProcessSkill(TEXT("Skill1")); }
void AMageCharacter::Skill2() { ProcessSkill(TEXT("Skill2")); }