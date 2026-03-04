#include "Project_Bang_Squad/Character/Base/BaseCharacter.h" 
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"
#include "Project_Bang_Squad/Character/StageBoss/StageBossGameMode.h" 
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "Project_Bang_Squad/Game/Stage/StagePlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Project_Bang_Squad/Game/Base/BSPlayerState.h"
#include "Project_Bang_Squad/Game/MiniGame/ArenaMiniGameMode.h"
#include "Project_Bang_Squad/Game/MiniGame/ArenaGameState.h"
#include "Project_Bang_Squad/UI/Enemy/EnemyNormalHPWidget.h"

// ====================================================================================
// [Section 1] 초기화 (Initialization)
// ====================================================================================

ABaseCharacter::ABaseCharacter()
{
    // --- 기본 네트워크 설정 ---
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);
    SetNetUpdateFrequency(100.0f);     // 서버 갱신 주기 (부드러운 움직임 보장)
    SetMinNetUpdateFrequency(66.0f);   

    Tags.Add(TEXT("Player")); // 아군 판별용 태그

    // --- 컨트롤러 및 이동 설정 ---
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = true;    // 캐릭터가 카메라 보는 방향으로 돌도록 설정
    bUseControllerRotationRoll = false;

    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        MoveComp->bOrientRotationToMovement = false; // 위 컨트롤러 세팅과 충돌하지 않게 false
        MoveComp->RotationRate = FRotator(0.f, 720.f, 0.f);
        MoveComp->MaxWalkSpeed = 550.f;
        MoveComp->JumpZVelocity = 500.f;
        MoveComp->AirControl = 0.5f;     // 점프 중 방향 전환 능력
        MoveComp->SetWalkableFloorAngle(60.f); // 최대 등반 가능 경사도
        MoveComp->LedgeCheckThreshold = 4.f;
        MoveComp->bMaintainHorizontalGroundVelocity = true;
    }

    // --- 핵심 컴포넌트 생성 ---
    HealthComp = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComp"));

    // 카메라 스프링암 (부드러운 시점 이동 및 벽 뚫림 방지)
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(GetRootComponent());
    SpringArm->TargetArmLength = 450.f;
    SpringArm->bUsePawnControlRotation = true;
    SpringArm->bEnableCameraLag = true; // 카메라 부드럽게 따라가기
    SpringArm->CameraLagSpeed = 10.f;
    SpringArm->bDoCollisionTest = true; 
    SpringArm->ProbeChannel = ECC_Visibility; // 가시성 채널 기준으로 벽을 감지해 줌인/아웃
    SpringArm->ProbeSize = 15.0f; 

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm);
    Camera->bUsePawnControlRotation = false;

    // --- 상점 아이템 소켓 및 메쉬 설정 ---
    AccessorySocketName = FName("Bip001-Head"); // 머리 장식 부착 기본 소켓
    ItemAttachParent = GetMesh();

    HeadAccessoryComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadAccessory"));
    HeadAccessoryComponent->SetupAttachment(GetMesh(), AccessorySocketName);
    HeadAccessoryComponent->SetCollisionProfileName(TEXT("NoCollision")); // 아이템이 캐릭터 물리나 조준을 가리지 않게 함

    HeadSkeletalComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HeadSkeletalComp"));
    HeadSkeletalComp->SetupAttachment(GetMesh(), AccessorySocketName); 
    HeadSkeletalComp->SetCollisionProfileName(TEXT("NoCollision"));

    // --- 본인 전용 머리 위 마커 (나한테만 보임) ---
    OverheadMarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OverheadMarkerMesh"));
    OverheadMarkerMesh->SetupAttachment(GetMesh(), FName("Bip001-Head"));
    OverheadMarkerMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 30.0f));
    OverheadMarkerMesh->SetRelativeScale3D(FVector(0.5f));
    OverheadMarkerMesh->SetVisibility(false); // 일단 숨겨두고 BeginPlay에서 로컬만 켬
    OverheadMarkerMesh->SetCollisionProfileName(TEXT("NoCollision"));

    // --- 아레나 적 머리 위 체력바 위젯 ---
    ArenaHPBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("ArenaHPBarWidget"));
    ArenaHPBarWidget->SetupAttachment(GetMesh(), FName("HeadSocket")); 
    ArenaHPBarWidget->SetWidgetSpace(EWidgetSpace::Screen); // 항상 화면 정면을 보도록 설정
    ArenaHPBarWidget->SetDrawSize(FVector2D(150.0f, 20.0f));
    ArenaHPBarWidget->SetRelativeLocation(FVector(30.f, 0.f, 0.f));
    ArenaHPBarWidget->SetVisibility(false);

    // 내부 상태 변수 초기화
    JumpCooldownTimer = 1.2f;
    UnlockedStageLevel = 1;
    bIsAttacking = false;           
    bIsAttackCoolingDown = false;   
    bIsDead = false;
}

void ABaseCharacter::BeginPlay()
{
    Super::BeginPlay();
    
    // 이동 관련 부드러운 네트워크 보간 설정
    if (GetCharacterMovement())
    {
        DefaultMaxWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
        GetCharacterMovement()->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
        GetCharacterMovement()->bNetworkSkipProxyPredictionOnNetUpdate = false;
    }
    
    // 메쉬 업데이트 최적화 (보이지 않아도 뼈대 계산 유지하여 애니메이션 튐 현상 방지)
    if (GetMesh())
    {
        GetMesh()->SetGenerateOverlapEvents(false);
        GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    }
    
    // 체력 컴포넌트의 사망 이벤트를 캐릭터의 OnDeath 함수와 연결
    if (HealthComp)
    {
        HealthComp->OnDead.AddDynamic(this, &ABaseCharacter::OnDeath);
    }
    
    // --- 맵 이름을 읽어와 현재 스테이지 해금 및 모드 설정 ---
    if (UWorld* World = GetWorld())
    {
        FString MapName = World->GetMapName();
        MapName.RemoveFromStart(World->StreamingLevelsPrefix); // PIE(에디터) 환경 접두사 제거
       
        if (MapName.Contains(TEXT("Stage1"))) UnlockedStageLevel = 1;
        else if (MapName.Contains(TEXT("Stage2"))) UnlockedStageLevel = 2;
        else if (MapName.Contains(TEXT("Stage3"))) UnlockedStageLevel = 3;
       
        // 미니게임이면 스킬 등을 제한하기 위해 0으로 세팅
        if (MapName.Contains(TEXT("Stage1_MiniGame")) || MapName.Contains(TEXT("Stage2_MiniGame")))
        {
            bIsMiniGameMode = true;
            UnlockedStageLevel = 0;
        }
       
        if (MapName.Contains(TEXT("Lobby"))) UnlockedStageLevel = 3;
    }

    // 내 화면(로컬)일 경우에만 머리 위 마커를 표시
    if (IsLocallyControlled() && OverheadMarkerMesh)
    {
        OverheadMarkerMesh->SetVisibility(true);
        OverheadMarkerMesh->SetCastShadow(false);
    }

    // 약간의 지연 후 PlayerState 정보를 바탕으로 스킨/장비 로드 (서버 동기화 시간 확보)
    FTimerHandle TimerHandle;
    GetWorldTimerManager().SetTimer(TimerHandle, this, &ABaseCharacter::UpdateAppearanceFromPlayerState, 0.2f, false);

    if (GetMesh())
    {
        DefaultSkinMaterial = GetMesh()->GetMaterial(0);
    }
}

void ABaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    // 바람 기믹으로 공중에 떴는지 여부를 클라이언트들에게 복제
    DOREPLIFETIME(ABaseCharacter, bIsWindFloating);
}

void ABaseCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // 경사로 미끄러짐 계산 (매 프레임)
    ApplySlopeSlide(DeltaTime);
    
    // 머리 위 마커가 항상 카메라를 정면으로 바라보도록 회전 (빌보드 효과)
    if (OverheadMarkerMesh)
    {
        if (APlayerCameraManager* CamManager = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0))
        {
            FRotator CamRot = CamManager->GetCameraRotation();
            CamRot.Add(0.0f, 0.0f, 180.0f); // 뒷면이 안 보이게 180도 뒤집기
            OverheadMarkerMesh->SetWorldRotation(CamRot);
        }
    }
}

// ====================================================================================
// [Section 2] 입력 및 조작 (Input & Controls)
// ====================================================================================

void ABaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // Enhanced Input System(향상된 입력 시스템) 매핑 컨텍스트 적용
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
            {
                if (DefaultIMC) Subsystem->AddMappingContext(DefaultIMC, 0);
            }
        }
    }
    
    // 각 키 바인딩 연결
    UEnhancedInputComponent* EIC = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);

    if (MoveAction) EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABaseCharacter::Move);
    if (LookAction) EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABaseCharacter::Look);
    if (JumpAction)
    {
        EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ABaseCharacter::Jump);
        EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ABaseCharacter::StopJumping);
    }
    if (AttackAction) EIC->BindAction(AttackAction, ETriggerEvent::Started, this, &ABaseCharacter::Attack);
    if (Skill1Action) EIC->BindAction(Skill1Action, ETriggerEvent::Started, this, &ABaseCharacter::Skill1);
    if (Skill2Action) EIC->BindAction(Skill2Action, ETriggerEvent::Started, this, &ABaseCharacter::Skill2);
    if (JobAbilityAction) EIC->BindAction(JobAbilityAction, ETriggerEvent::Started, this, &ABaseCharacter::JobAbility);
    if (ZoomAction) EIC->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &ABaseCharacter::Zoom);
    if (InteractionAction) EIC->BindAction(InteractionAction, ETriggerEvent::Started, this, &ABaseCharacter::Interact);
}

void ABaseCharacter::Move(const FInputActionValue& Value)
{
    const FVector2D Input = Value.Get<FVector2D>();
    if (!Controller) return;
    
    // 카메라가 바라보는 방향을 기준으로 앞뒤/좌우 이동 계산
    const FRotator ControlRot = Controller->GetControlRotation();
    const FRotator YawRot(0.f, ControlRot.Yaw, 0.f);
    const FVector ForwardDir = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
    const FVector RightDir = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
    
    AddMovementInput(ForwardDir, Input.Y);
    AddMovementInput(RightDir, Input.X);
}

void ABaseCharacter::Look(const FInputActionValue& Value)
{
    const FVector2D Input = Value.Get<FVector2D>();
    AddControllerYawInput(Input.X);   // 마우스 좌우 (카메라 회전)
    AddControllerPitchInput(-Input.Y); // 마우스 상하 (언리얼 기본 설정상 반전 필요)
}

void ABaseCharacter::Jump()
{
    if (bJumpRestricted) return; // 기믹에 의해 강제 제한된 경우 무시
    
    if (bCanJump && !GetCharacterMovement()->IsFalling())
    {
        Super::Jump(); // 언리얼 기본 점프 호출
        
        // 무한 점프 방지를 위한 쿨타임 적용
        if (JumpCooldownTimer > 0.0f)
        {
            bCanJump = false;
            FTimerHandle H;
            GetWorldTimerManager().SetTimer(H, this, &ABaseCharacter::ResetJump, JumpCooldownTimer, false);
        }
    }
}

void ABaseCharacter::ResetJump() { bCanJump = true; }

void ABaseCharacter::Zoom(const FInputActionValue& Value)
{
    // 마우스 휠 입력값 (+1.0 또는 -1.0)
    float InputValue = Value.Get<float>();
    
    if (SpringArm && InputValue != 0.0f)
    {
        // 휠 방향에 따라 카메라 길이 계산 후 제한(Clamp) 구역 안에서 적용
        float CurrentLength = SpringArm->TargetArmLength;
        float NewLength = CurrentLength + (InputValue * -ZoomStep);
        NewLength = FMath::Clamp(NewLength, MinTargetArmLength, MaxTargetArmLength);
        SpringArm->TargetArmLength = NewLength;
    }
}

void ABaseCharacter::Interact()
{
    if (bIsDead) return;

    // F키를 눌렀을 때 내 캡슐 범위 내에 있는 상인(Merchant) 찾기
    TArray<AActor*> OverlappingActors;
    GetOverlappingActors(OverlappingActors);

    for (AActor* Actor : OverlappingActors)
    {
        if (Actor->ActorHasTag(TEXT("Merchant")))
        {
            // 상인을 찾으면 컨트롤러에게 상점 UI를 띄우라고 명령
            if (ABSPlayerController* PC = Cast<ABSPlayerController>(GetController()))
            {
                PC->ToggleShopUI();
            }
            return; // 한 명과 상호작용했으면 종료
        }
    }
}

// ====================================================================================
// [Section 3] 전투 및 스킬 (Combat & Skills)
// ====================================================================================

bool ABaseCharacter::CanAttack() const
{
    // 살아있고, 쿨타임도 안 돌고, 현재 다른 모션 재생 중이 아닐 때만 true
    return !bIsDead && !bIsAttackCoolingDown && !bIsAttacking;
}

void ABaseCharacter::StartAttackCooldown()
{
    bIsAttackCoolingDown = true;
    
    // 설정된 시간 뒤에 쿨타임 변수 해제
    GetWorld()->GetTimerManager().SetTimer(
        AttackCooldownTimerHandle,
        this,
        &ABaseCharacter::ResetAttackCooldown,
        AttackCooldownTime,
        false);
}

void ABaseCharacter::ResetAttackCooldown()
{
    bIsAttackCoolingDown = false;
}

void ABaseCharacter::Attack()
{
    // 자식 클래스(직업별 캐릭터)에서 오버라이드 됨. 
    // 미니게임 상태일 경우 공용 투척 액션 실행.
    if (GetWorld()->GetGameState<AArenaGameState>())
    {
        ArenaThrowAction();
        return;
    }
}

// 자식에서 오버라이드할 빈 껍데기 함수들
void ABaseCharacter::Skill1() { if (GetWorld()->GetGameState<AArenaGameState>()) return; }
void ABaseCharacter::Skill2() { if (GetWorld()->GetGameState<AArenaGameState>()) return; }
void ABaseCharacter::JobAbility() { if (GetWorld()->GetGameState<AArenaGameState>()) return; }

void ABaseCharacter::PlayActionMontage(UAnimMontage* MontageToPlay)
{
    if (!MontageToPlay) return;
    
    if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
    {
        // 모션 시작 시 상태 잠금 (평타 등 중복 실행 방지)
        bIsAttacking = true;
        AnimInstance->Montage_Play(MontageToPlay);
       
        // 몽타주가 끝날 때 OnAttackMontageEnded를 호출해달라고 델리게이트(예약) 등록
        FOnMontageEnded EndDelegate;
        EndDelegate.BindUObject(this, &ABaseCharacter::OnAttackMontageEnded);
        AnimInstance->Montage_SetEndDelegate(EndDelegate, MontageToPlay);
    }
}

void ABaseCharacter::Server_PlayActionMontage_Implementation(UAnimMontage* MontageToPlay)
{
    Multicast_PlayActionMontage(MontageToPlay); // 서버가 받아서 모두에게 방송
}

void ABaseCharacter::Multicast_PlayActionMontage_Implementation(UAnimMontage* MontageToPlay)
{
    // 로컬 예측: 이미 내 컴퓨터에서 직접 몽타주를 틀었다면, 서버가 보낸 방송은 무시! (이중 재생 덜덜거림 방지)
    if (MontageToPlay && !IsLocallyControlled())
    {
        PlayActionMontage(MontageToPlay);
    }
}

void ABaseCharacter::Server_StopActionMontage_Implementation(UAnimMontage* MontageToStop, float BlendOutTime)
{
    Multicast_StopActionMontage(MontageToStop, BlendOutTime);
}

void ABaseCharacter::Multicast_StopActionMontage_Implementation(UAnimMontage* MontageToStop, float BlendOutTime)
{
    // 로컬 클라이언트는 이미 정지했으므로 무시 (사망 몽타주는 강제 정지 방지)
    if (!IsLocallyControlled()) 
    {
        if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
        {
            if (AnimInstance->GetCurrentActiveMontage() != DeathMontage)
            {
                AnimInstance->Montage_Stop(BlendOutTime, MontageToStop);
            }
        }
    }
}

void ABaseCharacter::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    // 몽타주 종료 또는 캔슬 시 행동 가능 상태 복구
    bIsAttacking = false;
}

// ====================================================================================
// 타이탄 잡기 기믹 연동 (더미 함수)
// ====================================================================================
void ABaseCharacter::SetThrownByTitan(bool bThrown, AActor* Thrower)
{
    bWasThrownByTitan = bThrown;
    TitanThrower = Thrower;
}

void ABaseCharacter::SetIsGrabbed(bool bGrabbed) { }

// ====================================================================================
// [Section 4] 체력 및 데미지, 사망 (Health, Damage & Death)
// ====================================================================================

float ABaseCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
    // 해킹 방지 및 이중 깎임 방지를 위해 서버(Authority)에서만 데미지 처리
    if (!HasAuthority()) return 0.0f;
    if (HealthComp && HealthComp->IsDead()) return 0.0f;
    
    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    
    // 체력 컴포넌트에 차감 위임
    if (HealthComp)
    {
        HealthComp->ApplyDamage(ActualDamage);
    }

    // 피격 시 현재 회복 중이던 것을 멈추고 쿨타임(RegenDelay) 처음부터 다시 시작
    if (ActualDamage > 0.0f)
    {
        GetWorldTimerManager().ClearTimer(RegenTickTimer);
        GetWorldTimerManager().SetTimer(RegenDelayTimer, this, &ABaseCharacter::StartHealthRegen, RegenDelay, false);
    }
    return ActualDamage;
}

void ABaseCharacter::StartHealthRegen()
{
    // 죽었거나 이미 풀피면 회복 타이머 가동 안함
    if (bIsDead || (HealthComp && HealthComp->IsFullHealth())) return;
    
    // 일정 간격(0.5초 등)마다 HealthRegenTick 반복 실행
    GetWorldTimerManager().SetTimer(RegenTickTimer, this, &ABaseCharacter::HealthRegenTick, RegenTickInterval, true);
}

void ABaseCharacter::HealthRegenTick()
{
    if (bIsDead || !HealthComp)
    {
        GetWorldTimerManager().ClearTimer(RegenTickTimer);
        return;
    }
    
    // 회복 중 풀피가 되면 타이머 중지
    if (HealthComp->IsFullHealth())
    {
        GetWorldTimerManager().ClearTimer(RegenTickTimer);
        return;
    }
    
    // (초당 회복량 * 주기) 만큼 실제로 힐 적용
    float HealAmount = RegenRate * RegenTickInterval;
    HealthComp->ApplyHeal(HealAmount);
}

void ABaseCharacter::OnDeath()
{
    if (bIsDead) return;
    bIsDead = true; 
    
    Client_ShowDeathSubtitle(FText::FromString(TEXT("사망했습니다. [Tab] 키를 눌러 관전 전환이 가능합니다.")), 5.0f);
    
    // 유저 입력 완전히 무시 (죽어서 못 움직임)
    if (Controller)
    {
        Controller->SetIgnoreMoveInput(true);
        Controller->SetIgnoreLookInput(true);
    }
    
    // 시체 처리 연출 멀티캐스트
    Multicast_Death();

    if (HasAuthority())
    {
        // 게임 모드에 따라 전멸 확인, 관전 모드 등 처리 분기
        if (UWorld* World = GetWorld())
        {
            AGameModeBase* GM = World->GetAuthGameMode();
            
            if (AStageBossGameMode* BossGM = Cast<AStageBossGameMode>(GM))
            {
                BossGM->OnPlayerDied(GetController());
            }
            else if (AArenaMiniGameMode* ArenaGM = Cast<AArenaMiniGameMode>(GM))
            {
                ArenaGM->OnPlayerDied(GetController());
            }
            else if (AStagePlayerController* PC = Cast<AStagePlayerController>(GetController()))
            {
                // 일반 스테이지는 2초 뒤 관전 카메라로 전환
                FTimerHandle Handle;
                World->GetTimerManager().SetTimer(Handle, PC, &AStagePlayerController::StartSpectating, 2.0f, false);
            }
        }
    }
    
    // 사망했으므로 회복 및 공격 관련 모든 타이머 정리
    GetWorldTimerManager().ClearTimer(RegenDelayTimer);
    GetWorldTimerManager().ClearTimer(RegenTickTimer);
    GetWorldTimerManager().ClearTimer(AttackCooldownTimerHandle);
    
    bIsAttackCoolingDown = false;
    bIsAttacking = false;
}

void ABaseCharacter::Multicast_Death_Implementation()
{
    // 1. 공중에 있었다면 바닥으로 떨어지도록 설정하고 속도 0으로 만듦
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->GravityScale = 1.0f; 
        GetCharacterMovement()->SetMovementMode(MOVE_Falling);
    }

    // 2. 캡슐 이외의 몸에 달린 모든 메쉬/장비 충돌 제거 (시체에 가로막힘 방지)
    TArray<UPrimitiveComponent*> AllComps;
    GetComponents<UPrimitiveComponent>(AllComps);

    for (UPrimitiveComponent* Comp : AllComps)
    {
        if (Comp == GetCapsuleComponent()) continue;

        Comp->SetCollisionProfileName(TEXT("NoCollision"));
        Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Comp->SetCollisionResponseToAllChannels(ECR_Ignore);
    }

    // 3. 메인 캡슐은 땅 밑으로 안 꺼지게 바닥(WorldStatic)만 밟도록 설정
    if (GetCapsuleComponent())
    {
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    }

    // 4. 사망 모션 재생 후, 모션이 끝날 즈음 애니메이션 강제 정지(Freeze)
    float MontageDuration = DeathMontage ? PlayAnimMontage(DeathMontage) : 2.0f;
    float FreezeDelay = (MontageDuration > 0.0f) ? (MontageDuration - 0.1f) : 0.0f;
    
    GetWorldTimerManager().SetTimer(DeathTimerHandle, this, &ABaseCharacter::FreezeAnimation, FreezeDelay, false);
}

void ABaseCharacter::FreezeAnimation()
{
    // Ragdoll 대신 마지막 쓰러진 포즈로 동상처럼 굳힘 (최적화)
    if (GetMesh())
    {
        GetMesh()->bPauseAnims = true;
        GetMesh()->bNoSkeletonUpdate = true;
    }
}

void ABaseCharacter::Landed(const FHitResult& Hit)
{
    Super::Landed(Hit);

    // 타이탄이 캐릭터를 집어 던졌을 때 바닥에 닿은 순간 폭발 데미지 기믹
    if (HasAuthority() && bWasThrownByTitan)
    {
        bWasThrownByTitan = false;
        GetCharacterMovement()->GravityScale = 1.0f; // 우주로 날아가지 않게 중력 원상복구
        GetCharacterMovement()->AirControl = 0.35f;

        FVector ImpactLoc = Hit.ImpactPoint;
        TArray<FHitResult> HitResults;
        FCollisionShape Sphere = FCollisionShape::MakeSphere(350.0f); // 폭발 반경 3.5m

        // 주변 액터 스캔
        bool bHit = GetWorld()->SweepMultiByChannel(
            HitResults, ImpactLoc, ImpactLoc, FQuat::Identity, ECC_Pawn, Sphere);

        if (bHit)
        {
            for (auto& Res : HitResults)
            {
                AActor* HitActor = Res.GetActor();
                // 나 자신이나 날 던진 타이탄은 데미지 제외
                if (!HitActor || HitActor == this || HitActor == TitanThrower) continue;

                if (ACharacter* HitChar = Cast<ACharacter>(HitActor))
                {
                    bool bIsAlly = HitActor->ActorHasTag("PlayerTeam");
                    
                    // 적군이면 강한 데미지 부여
                    if (!bIsAlly)
                    {
                        UGameplayStatics::ApplyDamage(HitActor, 100.0f,
                            TitanThrower ? TitanThrower->GetInstigatorController() : nullptr,
                            TitanThrower, UDamageType::StaticClass());
                    }

                    // 적/아군 상관없이 폭발의 충격파로 밀쳐냄
                    FVector KnockDir = HitActor->GetActorLocation() - ImpactLoc;
                    KnockDir.Normalize();
                    FVector LaunchForce = (KnockDir * 1000.0f) + FVector(0, 0, 500.0f);
                    HitChar->LaunchCharacter(LaunchForce, true, true);
                }
            }
        }
    }
}

// ====================================================================================
// [Section 5] 상점 및 외형 시스템 (Shop & Appearance)
// ====================================================================================

void ABaseCharacter::EquipShopItem(const FShopItemData& ItemData)
{
    if (ItemData.StaticMesh == nullptr && ItemData.SkeletalMesh == nullptr)
    {
        if (HeadAccessoryComponent)
        {
            HeadAccessoryComponent->SetStaticMesh(nullptr);
            HeadAccessoryComponent->SetVisibility(false);
        }
        if (HeadSkeletalComp)
        {
            HeadSkeletalComp->SetSkeletalMesh(nullptr);
            HeadSkeletalComp->SetVisibility(false);
        }
        return; // 제거 로직 수행 후 종료
    }

    if (HeadAccessoryComponent) HeadAccessoryComponent->SetVisibility(false);
    if (HeadSkeletalComp) HeadSkeletalComp->SetVisibility(false);

    USceneComponent* TargetParent = ItemAttachParent ? ItemAttachParent : GetMesh();

    if (ItemData.SkeletalMesh != nullptr && HeadSkeletalComp)
    {
        HeadSkeletalComp->SetSkeletalMesh(ItemData.SkeletalMesh);
        HeadSkeletalComp->AttachToComponent(TargetParent, FAttachmentTransformRules::SnapToTargetIncludingScale, AccessorySocketName);
        HeadSkeletalComp->SetRelativeTransform(ItemData.AdjustTransform); // 아이템별 미세 위치 보정 적용

        if (TargetParent != GetMesh()) HeadSkeletalComp->AddLocalOffset(FVector(0.0f, 0.0f, 18.0f));
        if (!CharacterSpecificAccessoryOffset.IsNearlyZero()) HeadSkeletalComp->AddLocalOffset(CharacterSpecificAccessoryOffset);

        HeadSkeletalComp->SetVisibility(true);

        if (ItemData.IdleAnimation)
        {
            HeadSkeletalComp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
            HeadSkeletalComp->PlayAnimation(ItemData.IdleAnimation, true);
        }
        else
        {
            HeadSkeletalComp->Stop();
        }
    }
    else if (ItemData.StaticMesh != nullptr && HeadAccessoryComponent)
    {
        HeadAccessoryComponent->SetStaticMesh(ItemData.StaticMesh);
        HeadAccessoryComponent->AttachToComponent(TargetParent, FAttachmentTransformRules::SnapToTargetIncludingScale, AccessorySocketName);
        HeadAccessoryComponent->SetRelativeTransform(ItemData.AdjustTransform);

        if (TargetParent != GetMesh()) HeadAccessoryComponent->AddLocalOffset(FVector(0.0f, 0.0f, 18.0f));
        if (!CharacterSpecificAccessoryOffset.IsNearlyZero()) HeadAccessoryComponent->AddLocalOffset(CharacterSpecificAccessoryOffset);

        HeadAccessoryComponent->SetVisibility(true);
    }
}

void ABaseCharacter::Server_EquipShopItem_Implementation(const FShopItemData& ItemData)
{
    Multicast_EquipShopItem(ItemData); // 클라이언트가 상점에서 사면 서버에 전송 후 모두에게 방송
}

void ABaseCharacter::Multicast_EquipShopItem_Implementation(const FShopItemData& ItemData)
{
    EquipShopItem(ItemData); 
}

void ABaseCharacter::EquipSkin(UMaterialInterface* NewSkin)
{
    if (GetMesh())
    {
        UMaterialInterface* MaterialToApply = NewSkin ? NewSkin : DefaultSkinMaterial;

        if (MaterialToApply)
        {
            GetMesh()->SetMaterial(0, MaterialToApply);
        }
    }
}

void ABaseCharacter::UpdateAppearanceFromPlayerState()
{
    ABSPlayerState* PS = GetPlayerState<ABSPlayerState>();
    if (!PS) return;

    // 머리 장식 처리
    if (PS->CurrentEquippedHeadID.IsNone())
    {
        EquipShopItem(FShopItemData()); // 빈 데이터로 제거
    }
    else if (ItemDataTable)
    {
        FShopItemData* Data = ItemDataTable->FindRow<FShopItemData>(PS->CurrentEquippedHeadID, TEXT("EquipHead"));
        if (Data) EquipShopItem(*Data);
    }

    // 스킨 처리
    if (PS->CurrentEquippedSkinID.IsNone())
    {
        // ID가 없으면 nullptr을 넘겨서 EquipSkin 내부에서 Default로 복구하게 함
        EquipSkin(nullptr);
    }
    else if (SkinDataTable)
    {
        FShopItemData* Data = SkinDataTable->FindRow<FShopItemData>(PS->CurrentEquippedSkinID, TEXT("EquipSkin"));
        if (Data) EquipSkin(Data->SkinMaterial);
    }
}

// ====================================================================================
// [Section 6] 이동 조작, 환경 기믹 및 버프/디버프 (Environment & Debuffs)
// ====================================================================================

void ABaseCharacter::SetJumpRestricted(bool bRestricted)
{
    bJumpRestricted = bRestricted;
    if (bRestricted) StopJumping(); // 제약 걸리면 공중에서라도 점프 로직 차단
}

void ABaseCharacter::ApplySlowDebuff(bool bActive, float Ratio)
{
    // 슬로우 늪/마법에 맞았을 때 속도 감소 처리
    if (!HasAuthority()) return;

    UCharacterMovementComponent* MoveComp = GetCharacterMovement();
    if (!MoveComp) return;

    if (bActive) 
    {
        if (CachedWalkSpeed <= 0.1f) CachedWalkSpeed = MoveComp->MaxWalkSpeed; // 원래 속도 기억
        CurrentSlowRatio = Ratio; // 감소 비율 (ex: 0.5면 반토막)
    }
    else
    {
        CurrentSlowRatio = 1.0f; // 원래대로 복구
        CachedWalkSpeed = 0.0f;
    }

    // 새 속도 적용 및 클라이언트들에 전송
    float TargetSpeed = DefaultMaxWalkSpeed * CurrentSlowRatio;
    MoveComp->MaxWalkSpeed = TargetSpeed;

    Multicast_SetMaxWalkSpeed(TargetSpeed, CurrentSlowRatio);
}

void ABaseCharacter::Multicast_SetMaxWalkSpeed_Implementation(float NewSpeed, float NewRatio)
{
    // 클라이언트의 예측 이동이 서버와 일치하도록 비율과 수치 동기화
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->MaxWalkSpeed = NewSpeed;
    }
    CurrentSlowRatio = NewRatio;
}

void ABaseCharacter::SetWindResistance(bool bEnable)
{
    // 바람 기믹(밀어내는 힘) 구역 진입 시 발바닥 마찰을 줄여 캐릭터가 부드럽게 밀리게 함
    UCharacterMovementComponent* MoveComp = GetCharacterMovement();
    if (!MoveComp) return;

    if (bEnable)
    {
        OriginalBrakingDeceleration = MoveComp->BrakingDecelerationWalking; // 원본 값 저장
        MoveComp->BrakingDecelerationWalking = 100.0f; // 브레이크 거의 풀어버림
    }
    else
    {
        MoveComp->BrakingDecelerationWalking = OriginalBrakingDeceleration; // 원상 복구
    }
}

void ABaseCharacter::OnRep_WindFloating()
{
    // 토네이도 기믹 등에 의해 캐릭터가 허공에 떠오를 때 처리
    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        if (bIsWindFloating)
        {
            MoveComp->SetMovementMode(MOVE_Falling); // 즉시 걷기 강제 취소 (발 떼기)
            MoveComp->GravityScale = 0.0f;           // 둥둥 떠다니도록 중력 무시
        }
        else
        {
            MoveComp->SetMovementMode(MOVE_Walking);
            MoveComp->GravityScale = 1.0f;           // 중력 정상화
        }
    }
}

void ABaseCharacter::ApplySlopeSlide(float DeltaTime)
{
    // 아주 가파른 경사(예: 피라미드 외벽)를 밟으면 캐릭터가 미끄러지도록 하는 물리 연산
    UCharacterMovementComponent* MoveComp = GetCharacterMovement();
    if (!MoveComp) return;

    float NormalSpeed = 550.0f * CurrentSlowRatio;

    // 공중일 때는 마찰력만 깎고 계산 종료 (오작동 방지)
    if (!MoveComp->CurrentFloor.bBlockingHit)
    {
        MoveComp->GroundFriction = 8.0f;
        return;
    }

    // 현재 밟고 있는 바닥의 경사도 각도 계산 (0도~90도)
    FVector FloorNormal = MoveComp->CurrentFloor.HitResult.Normal;
    float SlopeAngle = FMath::RadiansToDegrees(FMath::Acos(FloorNormal.Z));

    // 미끄러질 방향 = 중력 방향 투영 벡터
    FVector GravityDir = FVector(0, 0, -1);
    FVector SlideDir = GravityDir - (FloorNormal * FVector::DotProduct(GravityDir, FloorNormal));
    SlideDir.Normalize();

    // 플레이어가 미끄럼틀을 타고 내려가는 중인지(Downhill) 판단
    FVector MoveDir = GetVelocity().GetSafeNormal();
    if (MoveDir.IsNearlyZero()) MoveDir = MoveComp->GetLastInputVector().GetSafeNormal();
    bool bIsDownhill = FVector::DotProduct(MoveDir, SlideDir) > 0.2f;

    // 30도 이상 가파른 경사일 경우 강제 미끄러짐 적용
    if (SlopeAngle >= 30.0f)
    {
        // 30~50도 사이에서 얼마나 가파른지 비율 0.0 ~ 1.0 산출
        float Alpha = FMath::GetMappedRangeValueClamped(FVector2D(30.f, 50.f), FVector2D(0.f, 1.f), SlopeAngle);
        float CurveAlpha = FMath::Sqrt(Alpha); // 선형이 아닌 곡선으로 미끄러짐 체감 극대화
        float CurrentAccel = CurveAlpha * MaxSlideAccel;

        // 경사가 가파를수록 발바닥 마찰력을 제거해버림
        float FrictionAlpha = FMath::GetMappedRangeValueClamped(FVector2D(40.f, 50.f), FVector2D(0.f, 1.f), SlopeAngle);
        MoveComp->GroundFriction = FMath::Lerp(2.0f, 0.5f, FrictionAlpha);
        MoveComp->BrakingDecelerationWalking = FMath::Lerp(500.0f, 0.0f, FrictionAlpha);

        if (bIsDownhill)
        {
            // 내려갈 땐 정상 이속 유지 (시원하게 미끄러지라고)
            MoveComp->MaxWalkSpeed = NormalSpeed;
        }
        else
        {
            // 억지로 올라가려고 할 땐 이속을 대폭 깎아서 (550 -> 200) 결국 못 올라가게 방해함
            float TargetMinSpeed = 200.0f * CurrentSlowRatio;
            MoveComp->MaxWalkSpeed = FMath::Lerp(NormalSpeed, TargetMinSpeed, Alpha);
        }

        // 실제 미끄러지는 힘 가하기
        MoveComp->Velocity += SlideDir * CurrentAccel * DeltaTime * 12.0f;
    }
    else
    {
        // 평지일 경우 모든 값을 정상으로 복구
        MoveComp->MaxWalkSpeed = NormalSpeed;
        MoveComp->GroundFriction = 8.0f;
        MoveComp->BrakingDecelerationWalking = 2048.0f;
    }
}

// ====================================================================================
// [Section 7] 미니게임 아레나 (MiniGame Arena)
// ====================================================================================

void ABaseCharacter::ArenaThrowAction()
{
    // 아레나 맵에서는 직업 스킬이 아닌 전용 폭탄 투척 등을 수행
    if (bIsDead || !bCanArenaThrow) return;
    
    bCanArenaThrow = false; // 연사 방지
    GetWorldTimerManager().SetTimer(ArenaThrowCooldownTimer, this, &ABaseCharacter::ResetArenaThrow, 1.5f, false);
    
    // 던지는 몽타주 재생
    if (ArenaThrowMontage) PlayActionMontage(ArenaThrowMontage);
    
    // 모션 중간(손을 뻗을 때)에 진짜 투사체가 생성되도록 타이머 지연 발동
    if (HasAuthority())
    {
        GetWorldTimerManager().SetTimer(
            ThrowDelayTimerHandle,
            this,
            &ABaseCharacter::Execute_ArenaThrowProjectile,
            ArenaThrowDelay,
            false
        );
    }
    else
    {
        Server_ArenaThrowAction();
    }
}

void ABaseCharacter::Server_ArenaThrowAction_Implementation() { ArenaThrowAction(); }
void ABaseCharacter::ResetArenaThrow() { bCanArenaThrow = true; }

void ABaseCharacter::Execute_ArenaThrowProjectile()
{
    if (!HasAuthority() || !ArenaProjectileClass) return;
    
    // 투사체 스폰 위치: 캐릭터 앞쪽 1M, 높이 0.5M 지점
    FVector CharLoc = GetActorLocation();
    FVector ForwardDir = GetActorForwardVector();
    FVector SpawnLoc = CharLoc + (ForwardDir * 100.f) + FVector(0.0f, 0.0f, 50.0f);
    
    // 날아가는 궤적은 마우스 십자선(Camera 방향)을 따라가게 설정
    FRotator SpawnRot = Camera ? Camera->GetComponentRotation() : GetActorRotation();
    
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = GetInstigator();
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn; // 무조건 생성 강제

    GetWorld()->SpawnActor<AActor>(ArenaProjectileClass, SpawnLoc, SpawnRot, SpawnParams);
}

void ABaseCharacter::ShowArenaHPBar()
{
    // 적 머리 위에 뜨는 위젯(로컬 플레이어는 내 머리 위에 띄울 필요 없으므로 리턴)
    if (IsLocallyControlled()) return;

    if (ArenaHPBarWidget)
    {
        // 렌더링 권한을 화면 호스트 플레이어에게 강제 귀속시켜 항상 잘 보이게 함
        if (APlayerController* LocalPC = GetWorld()->GetFirstPlayerController())
        {
            if (ULocalPlayer* LocalPlayer = LocalPC->GetLocalPlayer())
            {
                ArenaHPBarWidget->SetOwnerPlayer(LocalPlayer);
            }
        }
        
        if (ArenaHPBarWidget->GetWidgetClass() == nullptr && HPBarWidgetClass)
        {
            ArenaHPBarWidget->SetWidgetClass(HPBarWidgetClass);
        }
       
        ArenaHPBarWidget->SetVisibility(true);
        ArenaHPBarWidget->SetHiddenInGame(false);

        // 위젯이 생성 안되어있으면 강제로 메모리 초기화
        if (!ArenaHPBarWidget->GetUserWidgetObject())
        {
            ArenaHPBarWidget->InitWidget();
        }

        // 실제 체력 데이터와 위젯의 UI 바를 동기화(바인딩)
        if (UUserWidget* UserWidget = ArenaHPBarWidget->GetUserWidgetObject())
        {
            if (UEnemyNormalHPWidget* HPWidget = Cast<UEnemyNormalHPWidget>(UserWidget))
            {
                if (HealthComp)
                {
                    HPWidget->UpdateHP(HealthComp->GetHealth(), HealthComp->GetMaxHealth());
                    HealthComp->OnHealthChanged.RemoveDynamic(HPWidget, &UEnemyNormalHPWidget::UpdateHP);
                    HealthComp->OnHealthChanged.AddDynamic(HPWidget, &UEnemyNormalHPWidget::UpdateHP);
                }
            }
        }
    }
}

void ABaseCharacter::HideArenaHPBar()
{
    if (ArenaHPBarWidget) ArenaHPBarWidget->SetVisibility(false);
}

// ====================================================================================
// [Section 8] 스킬 및 UI 유틸리티 (Skills & UI Utility)
// ====================================================================================

bool ABaseCharacter::IsSkillUnlocked(int32 RequiredStage)
{
    // 아레나 및 미니게임에서는 레벨과 상관없이 스킬 차단
    if (GetWorld() && GetWorld()->GetGameState<AArenaGameState>()) return false;
    if (bIsMiniGameMode) return false;
    
    // 요구 스테이지가 0이면(기본 스킬) 무조건 허용
    if (RequiredStage == 0) return true;
    
    return UnlockedStageLevel >= RequiredStage; // 진행 중인 맵 레벨 이상이어야 스킬 사용 허가
}

void ABaseCharacter::TriggerSkillCooldown(int32 SkillIndex, float CooldownTime)
{
    // 스킬을 사용했을 때 UMG 위젯(HUD)에서 타이머 애니메이션을 재생하라고 델리게이트 알림
    if (OnSkillCooldownChanged.IsBound())
    {
        OnSkillCooldownChanged.Broadcast(SkillIndex, CooldownTime);
    }
}

void ABaseCharacter::Client_TriggerSkillCooldown_Implementation(int32 SkillIndex, float CooldownTime)
{
    TriggerSkillCooldown(SkillIndex, CooldownTime); // 서버 명령을 받아 로컬 UI 갱신
}

UTexture2D* ABaseCharacter::GetSkillIconByRowName(FName RowName)
{
    // UI에서 아이콘을 그려야 할 때 데이터 테이블에서 사진 파일(Texture2D)을 찾아 반환
    if (!SkillDataTable) return nullptr;
    static const FString ContextString(TEXT("GetSkillIcon"));
    FSkillData* Data = SkillDataTable->FindRow<FSkillData>(RowName, ContextString);
    return Data ? Data->Icon : nullptr;
}

FText ABaseCharacter::GetSkillNameTextByRowName(FName RowName)
{
    // 스킬의 로컬라이징(다국어 번역) 가능 텍스트 이름 반환
    if (!SkillDataTable) return FText::GetEmpty();
    static const FString ContextString(TEXT("GetSkillName"));
    FSkillData* Data = SkillDataTable->FindRow<FSkillData>(RowName, ContextString);
    return Data ? FText::FromName(Data->SkillName) : FText::FromString(TEXT("Unknown"));
}

bool ABaseCharacter::IsSkillUnlockedByRowName(FName RowName)
{
    // 특정 UI 버튼(스킬)을 회색 자물쇠로 잠글지 여부를 판단
    if (!SkillDataTable) return false;
    static const FString ContextString(TEXT("CheckUnlockStatus"));
    FSkillData* Data = SkillDataTable->FindRow<FSkillData>(RowName, ContextString);
    return Data ? IsSkillUnlocked(Data->RequiredStage) : false;
}

void ABaseCharacter::Client_ShowDeathSubtitle_Implementation(const FText& Message, float Duration)
{
    // 1. 내 화면(로컬 플레이어 컨트롤러) 가져오기
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC || !PC->IsLocalController() || !SubtitleWidgetClass) return;
    
    // 2. 위젯 생성 및 화면에 띄우기
    UUserWidget* SubtitleWidget = CreateWidget<UUserWidget>(PC, SubtitleWidgetClass);
    if (SubtitleWidget)
    {
        SubtitleWidget->AddToViewport();
        
        // 3. 블루프린트의 ShowSubtitle 함수 호출
        UFunction* ShowFunc = SubtitleWidget->FindFunction(FName("ShowSubtitle"));
        if (ShowFunc)
        {
            struct { FText TextParams; } Params;
            Params.TextParams = Message;
            SubtitleWidget->ProcessEvent(ShowFunc, &Params);
            
        }
        
        TWeakObjectPtr<UUserWidget> WeakSubtitleWidget = SubtitleWidget;
        
        // 4. 지정된 시간 뒤에 삭제
        FTimerHandle RemoveTimer;
        GetWorldTimerManager().SetTimer(RemoveTimer, [SubtitleWidget]()
        {
            if (IsValid(SubtitleWidget))
            {
                SubtitleWidget->RemoveFromParent();
            }
        }, Duration, false);
    }

}
