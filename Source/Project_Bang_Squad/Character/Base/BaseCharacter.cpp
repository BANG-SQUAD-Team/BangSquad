#include "BaseCharacter.h" 
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"
#include "Project_Bang_Squad/Character/StageBoss/StageBossGameMode.h" 
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "TimerManager.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h" // [필수] 데미지 처리를 위해 필요
#include "Project_Bang_Squad/Game/Stage/StagePlayerController.h"

ABaseCharacter::ABaseCharacter()
{
	// 플레이어 태그 등록
	Tags.Add(TEXT("Player"));
	
	HealthComp = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComp"));
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bOrientRotationToMovement = true;
	MoveComp->RotationRate = FRotator(0.f, 720.f, 0.f);
	MoveComp->MaxWalkSpeed = 550.f;
	MoveComp->JumpZVelocity = 500.f;
	MoveComp->AirControl = 0.5f;
	
	JumpCooldownTimer = 1.2f;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetRootComponent());
	SpringArm->TargetArmLength = 450.f;
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 10.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
	Camera->bUsePawnControlRotation = false;

	UnlockedStageLevel = 1;

	//복제 설정
	bReplicates = true;
	SetReplicateMovement(true);
	NetUpdateFrequency = 100.0f;     // 1초에 100번 상태 갱신 시도 (서버 -> 클라)
	MinNetUpdateFrequency = 66.0f;   // 최소 66번은 보장 (프레임 방어)
}

void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	// HealthComponent가 있으면 '사망 이벤트'를 내 함수 OnDeath에 연결
	if (HealthComp)
	{
		HealthComp->OnDead.AddDynamic(this, &ABaseCharacter::OnDeath);
	}
}


void ABaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

float ABaseCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
	// 1. 서버가 아니면 무시 (해킹 방지 및 동기화 주체 확인)
	if (!HasAuthority()) return 0.0f;
	// 2. 이미 죽었으면 무시
	if (HealthComp && HealthComp->IsDead()) return 0.0f;
	// 3. 실제 데미지 적용
	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	// 체력 감소 (HealthComp가 알아서 처리하도록 위임)
	if (HealthComp)
	{
		HealthComp->ApplyDamage(ActualDamage);
	}

	// 맞았으니 회복 중단 & 대기 타이머 리셋
	if (ActualDamage > 0.0f)
	{
		// 1. 진행 중이던 회복 틱 정지
		GetWorldTimerManager().ClearTimer(RegenTickTimer);
		// 2. 대기 타이머 재설정 
		GetWorldTimerManager().SetTimer(RegenDelayTimer, this, &ABaseCharacter::StartHealthRegen, RegenDelay, false);
	}
	return ActualDamage;
}

void ABaseCharacter::StartHealthRegen()
{
	// 이미 죽었거나 체력이 꽉 차있으면 실행 X
	if (bIsDead) return;
	if (HealthComp && HealthComp->IsFullHealth()) return;
	
	// 일정 간격마다 HealthRegenTick 함수 반복 실행
	GetWorldTimerManager().SetTimer(RegenTickTimer, this, &ABaseCharacter::HealthRegenTick, RegenTickInterval, true);
	
}

void ABaseCharacter::HealthRegenTick()
{
	if (bIsDead || !HealthComp)
	{
		GetWorldTimerManager().ClearTimer(RegenTickTimer);
		return;
	}
	
	// 만약 이미 풀피라면? -> 타이머 끄기
	if (HealthComp->IsFullHealth())
	{
		GetWorldTimerManager().ClearTimer(RegenTickTimer);
		return;
	}
	
	// 실제 회복 (초당 회복량 * 간격)
	float HealAmount = RegenRate * RegenTickInterval;
	HealthComp->ApplyHeal(HealAmount);
}

void ABaseCharacter::OnDeath()
{
	// 1. 중복 사망 방지
	if (bIsDead) return;
	bIsDead = true; 
    
	// 2. [서버 로직] 컨트롤러 입력 차단 (서버가 입력 무시)
	if (Controller)
	{
		Controller->SetIgnoreMoveInput(true);
		Controller->SetIgnoreLookInput(true);
	}
    
	// 3. [핵심] 모든 클라이언트(나 포함)에게 "충돌 끄고 죽는 연기 해!"라고 방송
	Multicast_Death();

	// 4. 게임 모드에 따라 다르게 처리 (보스전 vs 일반 스테이지)
	if (HasAuthority())
	{
		UWorld* World = GetWorld();
		if (!World) return;
		
		AGameModeBase* GM = World->GetAuthGameMode();
		
		// [A] 보스전인가? -> 데스카운트 차감 요청
		if (AStageBossGameMode* BossGM = Cast<AStageBossGameMode>(GM))
		{
			// BossGM이 알아서 부활타이머를 돌리거나 전멸을 체크함
			BossGM->OnPlayerDied(GetController());
		}
		
		//[B] 일반 스테이지인가? -> 관전 모드 전환
		else if (AStagePlayerController* PC = Cast<AStagePlayerController>(GetController()))
		{
			FTimerHandle Handle;
			
			World->GetTimerManager().SetTimer(Handle, PC, &AStagePlayerController::StartSpectating,
				2.0f, false);
		}
	}
	
	// 5. 죽었으니 회복 관련 모든 타이머 종료
	GetWorldTimerManager().ClearTimer(RegenDelayTimer);
	GetWorldTimerManager().ClearTimer(RegenTickTimer);
	
}

void ABaseCharacter::Multicast_Death_Implementation()
{
    // 1. 이동 정지
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->GravityScale = 1.0f; 
        GetCharacterMovement()->SetMovementMode(MOVE_Falling);
    }

    // =================================================================
    // 2. [최종 해결책] 몸에 붙은 "모든" 컴포넌트를 찾아서 충돌 끄기
    // =================================================================
    // 방패, 칼, 날개, 꼬리 등... Capsule 외에 붙어있는 모든 걸 찾아냅니다.
    TArray<UPrimitiveComponent*> AllComps;
    GetComponents<UPrimitiveComponent>(AllComps);

    for (UPrimitiveComponent* Comp : AllComps)
    {
        // 캡슐 컴포넌트(몸통)는 아래에서 따로 설정할 거니까 건너뜀 (바닥 지지용)
        if (Comp == GetCapsuleComponent()) continue;

        // 나머지는 전부 충돌 끄기 (방패, 무기, 메쉬 포함)
        Comp->SetCollisionProfileName(TEXT("NoCollision"));
        Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Comp->SetCollisionResponseToAllChannels(ECR_Ignore);
    }

    // =================================================================
    // 3. 캡슐(몸통) 설정: "바닥만 밟고, 나머지는 통과"
    // =================================================================
    if (GetCapsuleComponent())
    {
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        
        // 일단 모든 채널 무시 (Reset)
        GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
        
        // 바닥(WorldStatic)만 밟아서 추락 방지
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
        
        // (혹시 바닥이 WorldDynamic인 경우도 있다면 추가)
        // GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
    }

    // 4. 사망 몽타주 재생
    float MontageDuration = 0.0f;
    if (DeathMontage)
    {
       MontageDuration = PlayAnimMontage(DeathMontage);
    }
    else
    {
        MontageDuration = 2.0f; 
    }

    float FreezeDelay = (MontageDuration > 0.0f) ? (MontageDuration - 0.1f) : 0.0f;
    GetWorldTimerManager().SetTimer(DeathTimerHandle, this, &ABaseCharacter::FreezeAnimation, FreezeDelay, false);
}

void ABaseCharacter::FreezeAnimation()
{
	if (GetMesh())
	{
		// 애니메이션 업데이트를 멈춤 -> 마지막 포즈로 고정됨
		GetMesh()->bPauseAnims = true;
		
		// 뼈대 업데이트도 멈춤 (성능 최적화)
		GetMesh()->bNoSkeletonUpdate = true;
	}
}

bool ABaseCharacter::CanAttack() const
{
	return !bIsDead && !bIsAttackCoolingDown && !bIsAttacking;
}

void ABaseCharacter::StartAttackCooldown()
{
	bIsAttackCoolingDown = true;
	
	// 설정된 시간 (AttackCooldownTime) 뒤에 쿨타임을 푼다
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

// 타이탄이 던질 때 호출
void ABaseCharacter::SetThrownByTitan(bool bThrown, AActor* Thrower)
{
	bWasThrownByTitan = bThrown;
	TitanThrower = Thrower;
}

// 잡혔을 때 상태 처리
void ABaseCharacter::SetIsGrabbed(bool bGrabbed)
{
	// 잡혀있는 동안 이동 입력을 막고 싶다면 여기에 로직 추가
}

// 땅에 닿았을 때 호출됨 
void ABaseCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (bWasThrownByTitan)
	{
		// 1. 상태 복구
		bWasThrownByTitan = false;
		GetCharacterMovement()->GravityScale = 1.0f; // 중력 원상복구
		GetCharacterMovement()->AirControl = 0.35f;

		// 2. 착지 지점 폭발 로직
		FVector ImpactLoc = Hit.ImpactPoint;
		TArray<FHitResult> HitResults;
		FCollisionShape Sphere = FCollisionShape::MakeSphere(350.0f); // 반경 3.5m

		bool bHit = GetWorld()->SweepMultiByChannel(
			HitResults, ImpactLoc, ImpactLoc, FQuat::Identity, ECC_Pawn, Sphere);

		if (bHit)
		{
			for (auto& Res : HitResults)
			{
				AActor* HitActor = Res.GetActor();
				if (!HitActor || HitActor == this || HitActor == TitanThrower) continue;

				if (ACharacter* HitChar = Cast<ACharacter>(HitActor))
				{
					// 아군/적군 판별 (태그 예시)
					bool bIsAlly = HitActor->ActorHasTag("PlayerTeam");

					if (!bIsAlly)
					{
						UGameplayStatics::ApplyDamage(HitActor, 100.0f,
							TitanThrower ? TitanThrower->GetInstigatorController() : nullptr,
							TitanThrower,
							UDamageType::StaticClass());
					}

					// 밀쳐내기 (Knockback)
					FVector KnockDir = HitActor->GetActorLocation() - ImpactLoc;
					KnockDir.Normalize();
					FVector LaunchForce = (KnockDir * 1000.0f) + FVector(0, 0, 500.0f);

					HitChar->LaunchCharacter(LaunchForce, true, true);
				}
			}
		}
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Purple, TEXT("Boom! Landed!"));
	}
}

/** 플레이어 입력 처리*/
void ABaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

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
}

void ABaseCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();
	if (!Controller) return;
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
	AddControllerYawInput(Input.X);
	AddControllerPitchInput(-Input.Y);
}


void ABaseCharacter::Jump()
{
	if (bCanJump && !GetCharacterMovement()->IsFalling())
	{
		Super::Jump();
		if (JumpCooldownTimer > 0.0f)
		{
			bCanJump = false;
			FTimerHandle H;
			GetWorldTimerManager().SetTimer(H, this, &ABaseCharacter::ResetJump, JumpCooldownTimer, false);
		}
	}
}

void ABaseCharacter::ResetJump() { bCanJump = true; }

bool ABaseCharacter::IsSkillUnlocked(int32 RequiredStage)
{
	if (RequiredStage == 0) return true;
	return UnlockedStageLevel >= RequiredStage;
}

void ABaseCharacter::ApplySlowDebuff(bool bActive, float Ratio)
{
	if (!HasAuthority()) return; // 서버만 명령 가능

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp) return;

	// 내 원래 속도를 '설계도(Default Object)'에서 가져옴
	// 이렇게 하면 캐릭터마다(전사, 메이지 등) 기본 속도가 달라도 정확히 복구됨
	float OriginalSpeed = 550.0f; // 비상용 기본값
	if (ACharacter* DefaultChar = GetClass()->GetDefaultObject<ACharacter>())
	{
		if (UCharacterMovementComponent* DefMove = DefaultChar->GetCharacterMovement())
		{
			OriginalSpeed = DefMove->MaxWalkSpeed;
		}
	}

	// 목표 속도 계산
	float TargetSpeed = bActive ? (OriginalSpeed * Ratio) : OriginalSpeed;

	// 방송!
	Multicast_SetMaxWalkSpeed(TargetSpeed);
}

void ABaseCharacter::Multicast_SetMaxWalkSpeed_Implementation(float NewSpeed)
{
	// 나(서버)와 저쪽(클라이언트) 모두 동시에 바뀜 -> 렉 없음!
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = NewSpeed;
	}
}

void ABaseCharacter::PlayActionMontage(UAnimMontage* MontageToPlay)
{
	if (!MontageToPlay) return;
	
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance)
	{
		// A. 상태 잠금
		bIsAttacking = true;
		
		// B. 몽타주 재생
		AnimInstance->Montage_Play(MontageToPlay);
		
		// C. 끝나면 알려라고 델리게이트 등록
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &ABaseCharacter::OnAttackMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, MontageToPlay);
	}
}

void ABaseCharacter::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// 몽타주가 끝나거나 취소되면 다시 행동 가능 상태로 복구
	bIsAttacking = false;
}
