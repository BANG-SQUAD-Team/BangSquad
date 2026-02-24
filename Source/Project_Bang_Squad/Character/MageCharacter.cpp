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
//  섹션 1: 생성자 및 초기화
// ====================================================================================

AMageCharacter::AMageCharacter()
{
    // 매 프레임마다 Tick 함수가 호출되도록 허용
    PrimaryActorTick.bCanEverTick = true;

    // 마우스로 시점을 돌릴 때 캐릭터 몸통이 즉각적으로 같이 돌지 않도록 설정
    bUseControllerRotationYaw = true;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;

    // 이동 관련 세팅 (이동 속도 및 캐릭터 회전 속도)
    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
       MoveComp->MaxWalkSpeed = 550.f;
       MoveComp->bOrientRotationToMovement = false; // 시점 기준 이동이므로 꺼둠
       MoveComp->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
    }

    // 기본 스탯 초기화
    AttackCooldownTime = 1.f;
    UnlockedStageLevel = 1;

    // 상호작용 타겟 포인터 초기화
    FocusedPillar = nullptr;
    CurrentTargetPillar = nullptr;

    // [기둥 조작용] 카메라가 부드럽게 줌인/줌아웃 되도록 돕는 타임라인 컴포넌트 생성
    CameraTimelineComp = CreateDefaultSubobject<UTimelineComponent>(TEXT("CameraTimelineComp"));

    // [보트 탑승용] 위에서 아래로 내려다보는 탑다운 시점의 스프링암 생성
    TopDownSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("TopDownSpringArm"));
    TopDownSpringArm->SetupAttachment(GetRootComponent());
    TopDownSpringArm->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f)); // 카메라가 수직 아래를 보게 설정
    TopDownSpringArm->TargetArmLength = 1200.0f; // 카메라 높이
    TopDownSpringArm->bDoCollisionTest = false; // 벽에 부딪혀도 카메라가 줌인되지 않게 함

    // [보트 탑승용] 탑다운 카메라 본체 생성
    TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
    TopDownCamera->SetupAttachment(TopDownSpringArm, USpringArmComponent::SocketName);
    TopDownCamera->SetAutoActivate(false); // 평소에는 꺼둠

    // [기본 시점] 부모 클래스에서 물려받은 기본 스프링암 설정
    if (SpringArm)
    {
       SpringArm->TargetOffset = FVector(0.0f, 0.0f, 150.0f); // 캐릭터 어깨 위쪽을 바라보도록 오프셋 지정
       SpringArm->ProbeSize = 12.0f;
       SpringArm->bUsePawnControlRotation = true; // 마우스 이동에 따라 카메라 회전
    }

    // [직업 아우라] 캐릭터 주변에 맴도는 이펙트 컴포넌트 생성
    JobAuraComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("JobAuraComp"));
    JobAuraComp->SetupAttachment(GetMesh(), TEXT("Bip001-Pelvis")); // 골반 위치에 부착
    JobAuraComp->bAutoActivate = false; // 스킬 사용 시에만 켜지도록 기본은 꺼둠

    // [무기 장착] 지팡이 등 무기가 붙을 루트 컴포넌트 생성
    WeaponRootComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Weapon_Root_R"));
    WeaponRootComp->SetupAttachment(GetMesh(), TEXT("Weapon_Root_R")); // 스켈레탈 메쉬의 소켓에 부착
    ItemAttachParent = WeaponRootComp; // 상점에서 산 아이템이 이곳에 붙도록 지정
    AccessorySocketName = FName("Acc_Socket");

    // 부모 클래스의 악세서리들을 메이지 전용 무기 위치로 재조정
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

    // 블루프린트 설정 오류를 막기 위해 기본 시점 설정을 코드로 강력하게 고정
    if (SpringArm)
    {
       SpringArm->bUsePawnControlRotation = true;
       SpringArm->bInheritPitch = true;
       SpringArm->bInheritYaw = true;
       SpringArm->bInheritRoll = false;

       // 기둥 줌인을 위해 원래의 카메라 거리와 오프셋을 기억해둠
       DefaultArmLength = SpringArm->TargetArmLength;
       DefaultSocketOffset = SpringArm->SocketOffset;
    }

    // 평상시엔 탑다운 카메라를 비활성화
    if (TopDownCamera) TopDownCamera->SetActive(false);

    // [카메라 줌인 타임라인 세팅] 
    FOnTimelineFloat TimelineProgress;
    TimelineProgress.BindDynamic(this, &AMageCharacter::CameraTimelineProgress);

    // 에디터에서 넣은 커브 데이터가 있으면 타임라인에 연결
    if (CameraCurve)
    {
       CameraTimelineComp->AddInterpFloat(CameraCurve, TimelineProgress);
    }
    else
    {
       // 커브가 없어도 에러가 나지 않도록 1초짜리 기본 타임라인으로 설정
       CameraTimelineComp->AddInterpFloat(nullptr, TimelineProgress);
       CameraTimelineComp->SetTimelineLength(1.0f);
    }

    // 타임라인 재생이 끝났을 때 호출될 이벤트 연결
    FOnTimelineEvent TimelineFinishedEvent;
    TimelineFinishedEvent.BindDynamic(this, &AMageCharacter::OnCameraTimelineFinished);
    CameraTimelineComp->SetTimelineFinishedFunc(TimelineFinishedEvent);
    CameraTimelineComp->SetLooping(false);

    // 무기 부착용 소켓 찾기 (마법 발사 위치를 찾기 위함)
    TArray<UStaticMeshComponent*> StaticComps;
    GetComponents<UStaticMeshComponent>(StaticComps);
    for (UStaticMeshComponent* Comp : StaticComps)
    {
       // 지팡이 소켓이나 끝부분 소켓이 있는 컴포넌트를 무기로 간주하여 캐싱
       if (Comp && (Comp->DoesSocketExist(TEXT("Weapon_Root_R")) || Comp->DoesSocketExist(TEXT("TipSocket")))) 
       { 
          CachedWeaponMesh = Comp; 
          break; 
       }
    }

    // 마법 지팡이 움직임 궤적(트레일) 이펙트 부착
    if (CachedWeaponMesh && StaffTrailVFX)
    {
       StaffTrailComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
          StaffTrailVFX,
          CachedWeaponMesh,
          NAME_None, 
          FVector::ZeroVector,
          FRotator::ZeroRotator,
          EAttachLocation::SnapToTarget,
          false
       );

       // 평타 칠 때만 켜야 하므로 평소엔 꺼둠
       if (StaffTrailComp)
       {
          StaffTrailComp->Deactivate(); 
       }
    }
    
    // 직업 아우라 이펙트 초기 셋업
    if (JobAuraComp && JobAuraVFX)
    {
       JobAuraComp->SetAsset(JobAuraVFX);
       JobAuraComp->SetRelativeScale3D(JobAuraScale); // 크기 조절
       JobAuraComp->Deactivate(); // 평소엔 꺼둠
    }
    
    // [성능 최적화] 스킬 사용 시마다 데이터를 찾지 않도록 게임 시작 시 미리 캐싱해둠
    if (SkillDataTable)
    {
       TArray<FName> RowNames = SkillDataTable->GetRowNames();
       for (const FName& RowName : RowNames)
       {
          static const FString ContextString(TEXT("MageSkillCacheContext"));
          FSkillData* Data = SkillDataTable->FindRow<FSkillData>(RowName, ContextString);
          if (Data)
          {
             SkillDataCache.Add(RowName, Data);
          }
       }
    }

    // [상호작용 탐지] 내 화면(로컬)에서만 0.1초마다 앞쪽의 상호작용 객체를 찾는 타이머 시작
    if (IsLocallyControlled())
    {
       GetWorldTimerManager().SetTimer(InteractionTimerHandle, this, &AMageCharacter::CheckInteractableTarget, 0.1f, true);
    }
}

void AMageCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // 키보드/마우스 입력 바인딩
    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
       // 직업 능력 (염력, 보트 등) 키 바인딩
       if (JobAbilityAction)
       {
          EIC->BindAction(JobAbilityAction, ETriggerEvent::Started, this, &AMageCharacter::JobAbility); // 누를 때
          EIC->BindAction(JobAbilityAction, ETriggerEvent::Completed, this, &AMageCharacter::EndJobAbility); // 뗄 때
          EIC->BindAction(JobAbilityAction, ETriggerEvent::Canceled, this, &AMageCharacter::EndJobAbility); // 취소될 때
       }
    }
}

// ====================================================================================
//  섹션 2: 상태 관리 및 틱
// ====================================================================================

void AMageCharacter::OnDeath()
{
    if (bIsDead) return;
    
    // 죽으면 모든 아우라와 이펙트 끄기
    Server_SetAuraActive(false);

    // 염력으로 무언가 조종 중이었다면 강제로 손을 놓게 함
    if (bIsPillarMode || bIsBoatMode)
    {
       EndJobAbility();
    }

    // 돌고 있던 스킬 타이머들 전부 취소
    GetWorldTimerManager().ClearTimer(ComboResetTimer);
    GetWorldTimerManager().ClearTimer(ProjectileTimerHandle);
    StopAnimMontage();

    Super::OnDeath();
}

void AMageCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 기둥 줌인 카메라 연출을 매 프레임 업데이트
    if (CameraTimelineComp)
    {
       CameraTimelineComp->TickComponent(DeltaTime, ELevelTick::LEVELTICK_TimeOnly, nullptr);
    }

    // 기둥을 조종 중이었는데 기둥이 무너지거나 사라지면 자동으로 조종 모드 해제
    if (bIsPillarMode)
    {
       if (!IsValid(CurrentTargetPillar) || CurrentTargetPillar->bIsFallen)
       {
          EndJobAbility();
          return;
       }
    }

    // 기둥 조종 중일 때 시선(카메라)을 기둥에 부드럽게 고정시킴
    if (bIsPillarMode && IsValid(CurrentTargetPillar) && !CurrentTargetPillar->bIsFallen)
    {
       LockOnPillar(DeltaTime);
    }
}

// ====================================================================================
//  섹션 3: 조작 및 상호작용
// ====================================================================================

void AMageCharacter::CheckInteractableTarget()
{
    // 나 자신이 조종하는 화면이 아니면 굳이 탐지할 필요 없으므로 타이머 파기
    if (!IsLocallyControlled())
    {
       GetWorldTimerManager().ClearTimer(InteractionTimerHandle);
       return;
    }

    // 이미 무언가를 조종하고 있다면 다른 대상을 찾지 않음
    if (bIsPillarMode || bIsBoatMode) return;

    // 플레이어가 기둥이나 다른 곳에 매달려있을 때는 마법 상호작용 하이라이트 해제
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
          {
             Interface->SetMageHighlight(false);
          }
          HoveredActor = nullptr;
       }
       return; 
    }

    // 카메라 전방을 향해 안보이는 레이저를 쏴서 물체 탐색
    FHitResult HitResult;
    FVector Start = Camera->GetComponentLocation();
    FVector End = Start + (Camera->GetForwardVector() * TraceDistance);
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this); // 나는 무시

    bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params);
    AActor* HitActor = HitResult.GetActor();

    // 맞은 물체가 기둥인지, 아니면 보트 같은 마법 상호작용 객체인지 파악
    APillar* HitPillar = Cast<APillar>(HitActor);
    bool bIsInterface = (HitActor && HitActor->Implements<UMagicInteractableInterface>());

    // 타겟 감지: 쓰러지지 않은 기둥일 경우
    if (HitPillar && !HitPillar->bIsFallen)
    {
       if (FocusedPillar != HitPillar)
       {
          // 예전에 바라보던 기둥이나 보트의 외곽선(하이라이트)을 끔
          if (HoveredActor)
          {
             if (IMagicInteractableInterface* Interface = Cast<IMagicInteractableInterface>(HoveredActor))
                Interface->SetMageHighlight(false);
             HoveredActor = nullptr;
          }
          if (FocusedPillar) FocusedPillar->PillarMesh->SetRenderCustomDepth(false);

          // 새로 쳐다본 기둥에 외곽선 켜기 (CustomDepth 활용)
          HitPillar->PillarMesh->SetRenderCustomDepth(true);
          HitPillar->PillarMesh->SetCustomDepthStencilValue(255);
          FocusedPillar = HitPillar;
       }
    }
    // 타겟 감지: 보트 등 인터페이스 적용 객체일 경우
    else if (bIsInterface)
    {
       if (HoveredActor != HitActor)
       {
          // 예전 타겟 외곽선 끄기
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
          // 새 타겟 외곽선 켜기
          if (IMagicInteractableInterface* Interface = Cast<IMagicInteractableInterface>(HitActor))
          {
             Interface->SetMageHighlight(true);
          }
          HoveredActor = HitActor;
       }
    }
    // 아무것도 안 쳐다볼 경우: 켜져있던 외곽선 전부 끄기
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
    // 마우스 이동 값을 받아옴
    FVector2D LookInput = Value.Get<FVector2D>();

    // [조작 분기 1] 보트나 회전 기둥을 조종 중일 때
    if (bIsBoatMode && CurrentControlledActor)
    {
       FVector MoveDir = FVector::ZeroVector;

       // 타겟이 보트일 경우: 카메라가 바라보는 방향을 기준으로 상하좌우 이동 계산
       if (Cast<AMagicBoat>(CurrentControlledActor))
       {
          if (TopDownCamera)
          {
             FVector ScreenUp = TopDownCamera->GetUpVector();
             FVector ScreenRight = TopDownCamera->GetRightVector();
             ScreenUp.Z = 0; // Z축(높이)은 무시하고 평면 이동만 처리
             ScreenRight.Z = 0;
             ScreenUp.Normalize();
             ScreenRight.Normalize();

             // 마우스 방향에 맞춰 방향 벡터 생성
             MoveDir = (ScreenUp * LookInput.Y) + (ScreenRight * LookInput.X);
          }
       }
       // 타겟이 회전 기둥일 경우: 마우스 위/아래 이동만 감지해서 회전 신호로 전달
       else
       {
          if (FMath::Abs(LookInput.Y) > 0.1f) // 미세한 손떨림 방지
          {
             MoveDir = FVector(0.f, LookInput.Y, 0.f);
          }
       }

       // 방향이 정해졌으면 서버로 해당 액터를 조종하라는 명령 전송
       if (!MoveDir.IsNearlyZero())
       {
          Server_InteractActor(CurrentControlledActor, MoveDir);
       }
    }
    // [조작 분기 2] 무너뜨릴 기둥을 붙잡고 있을 때
    else if (bIsPillarMode)
    {
       if (IsValid(CurrentTargetPillar) && !CurrentTargetPillar->bIsFallen)
       {
          // 기둥이 요구하는 마우스 꺾기 방향(좌/우)으로 마우스를 세게 움직였는지 확인
          if (FMath::Abs(LookInput.X) > 0.5f && FMath::Sign(LookInput.X) == CurrentTargetPillar->RequiredMouseDirection)
          {
             // 맞게 꺾었다면 서버에 기둥 파괴를 명령하고 조종 모드 해제
             Server_TriggerPillarFall(CurrentTargetPillar);
             EndJobAbility();
          }
       }
    }
    // [조작 분기 3] 평상시 마우스 화면 회전
    else
    {
       Super::Look(Value);
    }
}

void AMageCharacter::CameraTimelineProgress(float Alpha)
{
    // 타임라인 진행도(Alpha)에 따라 카메라 거리를 부드럽게 줌인/줌아웃
    if (!SpringArm) return;
    float NewArmLength = FMath::Lerp(DefaultArmLength, 1200.f, Alpha);
    SpringArm->TargetArmLength = NewArmLength;
    
    FVector NewOffset = FMath::Lerp(DefaultSocketOffset, FVector::ZeroVector, Alpha);
    SpringArm->SocketOffset = NewOffset;
}

void AMageCharacter::OnCameraTimelineFinished()
{
    // 줌인 연출이 끝났을 때 커서나 불필요한 입력을 차단하여 온전히 기둥 조작에 집중하게 함
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
    // 기둥 조종 시 카메라가 자동으로 기둥의 중심(상단)을 향해 부드럽게 돌아가도록 락온 연출
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

// ====================================================================================
//  섹션 4: 전투 및 스킬 시스템
// ====================================================================================

void AMageCharacter::Attack()
{
    if (!CanAttack()) return;

    // 평타 1타, 2타 모션 교차 사용 (콤보)
    FName SkillName = (CurrentComboIndex == 0) ? TEXT("Attack_A") : TEXT("Attack_B");
    ProcessSkill(SkillName);
    StartAttackCooldown();

    CurrentComboIndex++;
    if (CurrentComboIndex > 1) CurrentComboIndex = 0;

    // 일정 시간 공격 안하면 콤보 초기화
    GetWorldTimerManager().ClearTimer(ComboResetTimer);
    GetWorldTimerManager().SetTimer(ComboResetTimer, this, &AMageCharacter::ResetCombo, 1.2f, false);
}

void AMageCharacter::ResetCombo() { CurrentComboIndex = 0; }
void AMageCharacter::Skill1() { if (!bIsDead) ProcessSkill(TEXT("Skill1")); }
void AMageCharacter::Skill2() { if (!bIsDead) ProcessSkill(TEXT("Skill2")); }

void AMageCharacter::ProcessSkill(FName SkillRowName, FVector TargetLocation)
{
    if (!CanAttack()) return;

    // 내가 직접 조종하는 화면이면 무조건 화면 중앙(크로스헤어) 좌표를 쏴야 할 타겟으로 계산
    if (TargetLocation.IsZero() && IsLocallyControlled())
    {
        TargetLocation = GetCrosshairTargetLocation();
    }

    // 데이터 테이블에서 스킬 데미지, 몽타주, 쿨타임 정보 조회
    FSkillData** FoundData = SkillDataCache.Find(SkillRowName);
    FSkillData* Data = (FoundData) ? *FoundData : nullptr;

    // 아직 쿨타임 중이면 스킬 발동 무시
    if (SkillTimers.Contains(SkillRowName))
    {
       if (GetWorldTimerManager().IsTimerActive(SkillTimers[SkillRowName])) return;
    }

    if (Data)
    {
       // 캐릭터 해금 레벨보다 스킬 레벨이 높으면 사용 불가
       if (!IsSkillUnlocked(Data->RequiredStage)) return;

       // 애니메이션 재생 (본인 화면 즉시 재생으로 반응속도 올림)
       if (Data->SkillMontage) PlayActionMontage(Data->SkillMontage);

       // 방장(서버)일 경우 로직 처리
       if (HasAuthority())
       {
          // 다른 사람 화면에서도 내 애니메이션 재생하라고 지시
          if (Data->SkillMontage) Server_PlayMontage(Data->SkillMontage);

          // 평타일 경우 지팡이 끝에 잔상(트레일) 이펙트 켜기
          if (SkillRowName == TEXT("Attack_A") || SkillRowName == TEXT("Attack_B"))
          {
             Multicast_SetTrailActive(true);
          }
          
          // [스킬 2 - 바위 소환 로직]
          if (SkillRowName == TEXT("Skill2")) 
          {
             Multicast_PlaySkill2VFX();
             
             if (Data->ProjectileClass)
             {
                float Dmg = Data->GetRandomizedDamage();
                FTimerDelegate TimerDel;
                TimerDel.BindUObject(this, &AMageCharacter::SpawnSkill2Rock, Data->ProjectileClass.Get(), Dmg);
                
                // 마법 캐스팅 시간(ActionDelay)이 설정되어 있으면 딜레이 후 소환
                if (Data->ActionDelay > 0.0f)
                   GetWorldTimerManager().SetTimer(RockSpawnTimerHandle, TimerDel, Data->ActionDelay, false);
                else
                   SpawnSkill2Rock(Data->ProjectileClass, Dmg);
             }
          }
          // [스킬 1 & 평타 - 투사체 발사 로직]
          else
          {
             // 스킬 1 발사 이펙트
             if (SkillRowName == TEXT("Skill1")) Multicast_PlaySkill1VFX();

             if (Data->ProjectileClass)
             {
                float Dmg = Data->GetRandomizedDamage();
                FTimerDelegate TimerDel;
                
                // 완벽하게 동기화된 클라이언트의 에임 지점(TargetLocation)을 넘겨서 투사체 생성
                TimerDel.BindUObject(this, &AMageCharacter::SpawnDelayedProjectile, Data->ProjectileClass.Get(), Dmg, TargetLocation);

                if (Data->ActionDelay > 0.0f)
                   GetWorldTimerManager().SetTimer(ProjectileTimerHandle, TimerDel, Data->ActionDelay, false);
                else
                   SpawnDelayedProjectile(Data->ProjectileClass, Dmg, TargetLocation);
             }
          }
       }
       // 팀원(클라이언트)일 경우
       else
       {
          // 내가 바라보는 에임 좌표를 서버로 쏴서 그쪽으로 투사체를 쏴달라고 요청
          Server_ProcessSkill(SkillRowName, TargetLocation);
       }

       // 쿨타임 타이머 및 UI 동기화 처리
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
    // 서버가 아니거나 생성할 클래스가 없으면 취소
    if (!HasAuthority() || !ProjectileClass) return;
    
    // 투사체를 쏘는 시점이므로 지팡이 잔상 끄기
    Multicast_SetTrailActive(false);

    // 투사체가 생성될 초기 위치 (지팡이 끝부분)
    FVector SpawnLoc;
    FName SocketName = TEXT("Weapon_Root_R");
    if (GetMesh() && GetMesh()->DoesSocketExist(SocketName))
       SpawnLoc = GetMesh()->GetSocketLocation(SocketName);
    else
       SpawnLoc = GetActorLocation() + (GetActorForwardVector() * 100.f);

    // 클라이언트가 전송한 조준점(TargetLocation)을 최종 목표로 설정
    FVector FinalTargetLoc = TargetLocation;

    // 만약 에러나 외부 요인으로 좌표값이 0이라면 캐릭터 앞쪽 허공을 쏘게 보정
    if (FinalTargetLoc.IsZero()) 
    {
        FinalTargetLoc = SpawnLoc + (GetActorForwardVector() * 2000.0f);
    }

    // 지팡이 끝점에서 목표 지점을 바라보는 회전각 계산
    FRotator SpawnRot = UKismetMathLibrary::FindLookAtRotation(SpawnLoc, FinalTargetLoc);
    
    // 포물선 궤적을 위해 약간 위쪽으로 각도(Pitch) 보정
    SpawnRot.Pitch += ProjectilePitchOffset; 
    
    // 액터 생성(Spawn) 처리
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = GetInstigator();
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(ProjectileClass, SpawnLoc, SpawnRot, SpawnParams);

    // 생성된 투사체에 데미지와 주인(마법사) 설정
    if (AMageProjectile* MyProjectile = Cast<AMageProjectile>(SpawnedActor))
    {
       MyProjectile->Damage = DamageAmount;
       MyProjectile->SetOwner(this); 
    }
}

// ====================================================================================
//  섹션 5: 직업 능력 (염력 조종)
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
    
    // [염력 모드 1] 기둥을 붙잡았을 때
    if (FocusedPillar)
    {
       if (TargetMontage)
       {
          PlayActionMontage(TargetMontage);
          Server_PlayMontage(TargetMontage);
       }

       bIsPillarMode = true;
       CurrentTargetPillar = FocusedPillar;

       // 카메라 시점을 기둥에 맞추기 위해 락온 설정
       if (SpringArm)
       {
          SpringArm->bUsePawnControlRotation = true;
          SpringArm->bInheritPitch = true;
          SpringArm->bInheritYaw = true;
          SpringArm->bInheritRoll = true;
       }
       if (CameraTimelineComp) CameraTimelineComp->Play(); // 줌인 재생
       
       bSuccess = true;
    }
    // [염력 모드 2] 마법 보트나 회전 기둥을 붙잡았을 때
    else if (HoveredActor)
    {
       AMagicBoat* TargetBoat = Cast<AMagicBoat>(HoveredActor);

       if (TargetMontage)
       {
          PlayActionMontage(TargetMontage);
          Server_PlayMontage(TargetMontage);
       }

       // 보트일 경우, 내가 현재 그 보트 위에 타고 있는지(MovementBase) 검사
       if (TargetBoat)
       {
          UPrimitiveComponent* BaseComponent = GetMovementBase();
          bool bIsRiding = (BaseComponent && BaseComponent->GetOwner() == TargetBoat);
          if (!bIsRiding) return; // 타고있지 않으면 조종 불가
       }

       bIsBoatMode = true;
       CurrentControlledActor = HoveredActor;

       // 타겟이 회전 기둥(MovePillar)이면 즉시 회전 신호를 한번 보냄
       if (Cast<AMovePillar>(HoveredActor))
       {
          Server_InteractActor(CurrentControlledActor, FVector(1.0f, 0.0f, 0.0f));
       }

       // 타겟이 보트면 탑승 상태를 알리고 카메라를 탑다운 시점으로 전환
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
    
    // 성공적으로 능력을 썼으면 멋지게 아우라 발동
    if (bSuccess)
    {
       Server_SetAuraActive(true);
    }
}

void AMageCharacter::EndJobAbility()
{
    UAnimMontage* TargetMontage = nullptr;

    FSkillData** FoundData = SkillDataCache.Find(FName("JobAbility"));
    FSkillData* Data = (FoundData) ? *FoundData : nullptr;

    if (Data) TargetMontage = Data->SkillMontage;

    // 조종을 그만두므로 재생하던 애니메이션 강제 정지
    if (TargetMontage)
    {
       StopAnimMontage(TargetMontage);
       Server_StopMontage(TargetMontage);
    }

    // 아우라 끄기
    Server_SetAuraActive(false);
    
    // 기둥 모드였으면 카메라 줌아웃(Reverse) 복구
    if (bIsPillarMode)
    {
       bIsPillarMode = false;
       CurrentTargetPillar = nullptr;
       if (CameraTimelineComp) CameraTimelineComp->Reverse();
    }

    // 보트 모드였으면 카메라 원래 시점 복구 및 물리 충돌 롤백
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

       // 객체 이동 중지 명령 전송
       if (CurrentControlledActor)
       {
          Server_InteractActor(CurrentControlledActor, FVector::ZeroVector);
       }

       bIsBoatMode = false;
       CurrentControlledActor = nullptr;
    }

    // 카메라 회전 제어 복구
    if (SpringArm)
    {
       SpringArm->bUsePawnControlRotation = true;
       SpringArm->bInheritPitch = true;
       SpringArm->bInheritYaw = true;
       SpringArm->bInheritRoll = true;
    }
}

void AMageCharacter::SpawnSkill2Rock(UClass* RockClass, float DamageAmount)
{
   Multicast_StopSkill2VFX();

    if (!HasAuthority() || !RockClass) return;
    
   //  무조건 앞쪽이 아니라, 에임(크로스헤어)이 가리키는 지점을 찾습니다.
   FVector AimLocation = GetCrosshairTargetLocation();
    
   //  에임 지점 근처의 바닥을 정밀하게 찾기 위해 위아래로 레이저를 쏩니다.
   FVector TraceStart = AimLocation + FVector(0.f, 0.f, 500.f);
   FVector TraceEnd = AimLocation - FVector(0.f, 0.f, 1000.f);
    
    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    
    FVector FinalSpawnLoc = TraceStart;
    
    // 바닥에 부딪힌 지점이 있으면 그곳에서 튀어나오게 설정
    if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, Params))
    {
       FinalSpawnLoc = HitResult.ImpactPoint; 
    }
    else
    {
       // 바닥이 없으면 허공이므로 스킬 실패 처리 (낭떠러지 등)
       if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("Skill Failed: No Ground Found!"));
       return;
    }
    
    FRotator SpawnRot = FRotator::ZeroRotator;
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;
    
    // 계산된 바닥 위치에 거대한 바위 생성
    AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(RockClass, FinalSpawnLoc, SpawnRot, SpawnParams);
    
    // 생성된 바위에 데미지 값과 주인 정보 세팅
    if (AMageSkill2Rock* Rock = Cast<AMageSkill2Rock>(SpawnedActor))
    {
       Rock->InitializeRock(DamageAmount, this);
    }
}

// ====================================================================================
//  섹션 6: 네트워크 및 기타 유틸리티 처리 (RPC)
// ====================================================================================

// 클라이언트가 서버에게 스킬 사용을 명령
void AMageCharacter::Server_ProcessSkill_Implementation(FName SkillRowName, FVector TargetLocation)
{
    ProcessSkill(SkillRowName, TargetLocation);
}

// 서버가 모든 클라이언트 화면에 몽타주를 틀라고 명령 (경유지)
void AMageCharacter::Server_PlayMontage_Implementation(UAnimMontage* MontageToPlay)
{
    Multicast_PlayMontage(MontageToPlay);
}

// 실제 모든 클라이언트 화면에서 몽타주 재생
void AMageCharacter::Multicast_PlayMontage_Implementation(UAnimMontage* MontageToPlay)
{
    // 내가 누른 스킬 애니메이션을 또 재생하면 모션이 끊기므로 로컬(나) 빼고 재생
    if (MontageToPlay && !IsLocallyControlled())
    {
       PlayAnimMontage(MontageToPlay);
    }
}

// 서버가 모든 클라이언트 화면의 몽타주를 중지하라고 명령
void AMageCharacter::Server_StopMontage_Implementation(UAnimMontage* MontageToStop)
{
    Multicast_StopMontage(MontageToStop);
}

// 실제 모든 클라이언트 화면에서 몽타주 중지
void AMageCharacter::Multicast_StopMontage_Implementation(UAnimMontage* MontageToStop)
{
    if (MontageToStop && !IsLocallyControlled())
    {
       StopAnimMontage(MontageToStop);
    }
}

// 서버에게 염력(보트, 기둥 등) 방향키 입력을 전달
void AMageCharacter::Server_InteractActor_Implementation(AActor* TargetActor, FVector Direction)
{
    if (TargetActor)
    {
       // 인터페이스를 통해 대상 객체의 스크립트 실행
       IMagicInteractableInterface* Interface = Cast<IMagicInteractableInterface>(TargetActor);
       if (Interface)
       {
          Interface->ProcessMageInput(Direction);
       }
    }
}

// 서버에게 기둥을 쓰러뜨리라고 명령
void AMageCharacter::Server_TriggerPillarFall_Implementation(APillar* TargetPillar)
{
    if (TargetPillar) TargetPillar->TriggerFall();
}

// 서버에게 보트 탑승 상태를 알림
void AMageCharacter::Server_SetBoatRideState_Implementation(AMagicBoat* Boat, bool bRiding)
{
    if (Boat) Boat->SetRideState(bRiding);
}

// 모든 클라이언트 화면에서 마법 지팡이 트레일(잔상) 이펙트 켜고 끄기
void AMageCharacter::Multicast_SetTrailActive_Implementation(bool bActive)
{
    if (StaffTrailComp)
    {
       if (bActive) StaffTrailComp->Activate(true);
       else StaffTrailComp->Deactivate();
    }
}

// 서버에게 직업 고유 아우라 이펙트를 켜달라고 요청
void AMageCharacter::Server_SetAuraActive_Implementation(bool bActive)
{
    Multicast_SetAuraActive(bActive);
}

// 모든 클라이언트 화면에서 직업 아우라 켜고 끄기
void AMageCharacter::Multicast_SetAuraActive_Implementation(bool bActive)
{
    if (JobAuraComp)
    {
       if (bActive) JobAuraComp->Activate(true);
       else JobAuraComp->Deactivate();
    }
}

// 마법 발사 시 손끝(지팡이)에서 터지는 이펙트
void AMageCharacter::Multicast_PlaySkill1VFX_Implementation()
{
    if (Skill1CastEffect)
    {
       // 지팡이를 들고 있으면 지팡이에서, 맨손이면 캐릭터 몸에서 터짐
       USceneComponent* AttachTarget = CachedWeaponMesh ? CachedWeaponMesh : Cast<USceneComponent>(GetMesh());
       FName SocketName = TEXT("Weapon_Root_R"); 

       UGameplayStatics::SpawnEmitterAttached(
          Skill1CastEffect,
          AttachTarget,
          SocketName,
          FVector::ZeroVector,
          FRotator::ZeroRotator,
          EAttachLocation::SnapToTarget,
          true // 한번 터지고 자동 메모리 삭제
       );
    }
}

// 피해량 처리 함수 오버라이드 (맞았을 때의 처리)
float AMageCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    // 부모 클래스의 기본 피격 체력 감소 로직 실행
    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    // 기둥이나 보트를 조종하다가 적에게 맞으면 즉시 캔슬되게 만듦
    if (ActualDamage > 0.0f && (bIsPillarMode || bIsBoatMode))
    {
       EndJobAbility();
    }

    return ActualDamage;
}

// 크로스헤어(화면 정중앙)가 가리키는 실제 3D 좌표를 레이저로 쏴서 알아내는 함수
FVector AMageCharacter::GetCrosshairTargetLocation()
{
    // 카메라가 없으면 어쩔 수 없이 내 몸 앞 10m 허공 반환
    if (!Camera) return GetActorLocation() + GetActorForwardVector() * 1000.f;

    // 카메라 위치에서 시작해서 카메라가 보는 방향(앞)으로 100m짜리 레이저 발사
    FVector CamLoc = Camera->GetComponentLocation();
    FVector CamForward = Camera->GetForwardVector();

    FVector TraceStart = CamLoc;
    FVector TraceEnd = CamLoc + (CamForward * 10000.0f); 

    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this); // 나 자신은 통과되게 무시

    // 화면 가운데 뭐가 있는지 레이캐스트 검사
    bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, Params);

    // 뭔가 맞았으면 그 부딪힌 좌표점 반환, 100m동안 아무것도 없었으면 100m 앞 허공 좌표 반환
    return bHit ? HitResult.ImpactPoint : TraceEnd;
}

// 모든 클라이언트에서 스킬 2 캐스팅 이펙트 켜기
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

// 모든 클라이언트에서 스킬 2 캐스팅 이펙트 끄기
void AMageCharacter::Multicast_StopSkill2VFX_Implementation()
{
   if (IsValid(Skill2CastComp))
   {
      Skill2CastComp->DeactivateSystem(); 
      Skill2CastComp = nullptr;
   }
}