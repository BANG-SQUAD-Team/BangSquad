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
//  섹션 1: 생성자 및 초기화 (Constructor & Initialization)
// ====================================================================================

AMageCharacter::AMageCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = 550.f;
		MoveComp->bOrientRotationToMovement = false;
		MoveComp->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	}

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
	
	// [여기서 미리 생성]
	JobAuraComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("JobAuraComp"));
    
	// 1. 일단 루트(캡슐)에 붙인다.
	JobAuraComp->SetupAttachment(GetMesh(), TEXT("Bip001-Pelvis"));
    
	// 2. 시작하자마자 켜지는거 방지
	JobAuraComp->bAutoActivate = false;
}

void AMageCharacter::BeginPlay()
{
	Super::BeginPlay();

	// [강제 적용] 블루프린트 설정 무시하고 코드로 강제 설정
	if (SpringArm)
	{
		SpringArm->bUsePawnControlRotation = true;
		SpringArm->bInheritPitch = true;
		SpringArm->bInheritYaw = true;
		SpringArm->bInheritRoll = false;

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

	// 1. 지팡이 찾기
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

	// 2. 트레일 컴포넌트 생성 및 부착
	if (CachedWeaponMesh && StaffTrailVFX)
	{
		StaffTrailComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
			StaffTrailVFX,
			CachedWeaponMesh,
			NAME_None, // 지팡이 전체를 따라가게 (나이아가라 안에서 소켓 지정)
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			false // Auto Destroy 끄기
		);

		if (StaffTrailComp)
		{
			StaffTrailComp->Deactivate(); // 일단 꺼둠
		}
	}
	
	if (JobAuraComp && JobAuraVFX)
	{
		// 1. 에셋 끼우기
		JobAuraComp->SetAsset(JobAuraVFX);

		// 2. 크기 조절
		JobAuraComp->SetRelativeScale3D(JobAuraScale);

		// 3. 끄고 대기
		JobAuraComp->Deactivate();
	}

	
	// [최적화] 데이터 테이블 캐싱
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

	FOnTimelineEvent TimelineFinishedEvent;
	TimelineFinishedEvent.BindDynamic(this, &AMageCharacter::OnCameraTimelineFinished);
	CameraTimelineComp->SetTimelineFinishedFunc(TimelineFinishedEvent);
	CameraTimelineComp->SetLooping(false);

	// [최적화] 인터랙션 감지 타이머 시작 (0.1초마다)
	GetWorldTimerManager().SetTimer(InteractionTimerHandle, this, &AMageCharacter::CheckInteractableTarget, 0.1f, true);
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
//  섹션 2: 상태 관리 및 틱 (State Management & Tick)
// ====================================================================================

void AMageCharacter::OnDeath()
{
	if (bIsDead) return;
	
	Server_SetAuraActive(false);

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

	// [최적화] CheckInteractableTarget 제거됨 (타이머가 대체)

	// 락온 로직 (타겟이 있을 때만 작동)
	if (bIsPillarMode && IsValid(CurrentTargetPillar) && !CurrentTargetPillar->bIsFallen)
	{
		LockOnPillar(DeltaTime);
	}
}

// ====================================================================================
//  섹션 3: 조작 및 인터랙션 (Control, Camera, Interaction)
// ====================================================================================

void AMageCharacter::CheckInteractableTarget()
{
	// 조종 중엔 타겟 변경 금지
	if (bIsPillarMode || bIsBoatMode) return;

	if (GetAttachParentActor() != nullptr)
	{
		// 켜져 있던 기둥 아웃라인 끄기
		if (FocusedPillar)
		{
			if (FocusedPillar->PillarMesh)
			{
				FocusedPillar->PillarMesh->SetRenderCustomDepth(false);
			}
			FocusedPillar = nullptr;
		}

		// 켜져 있던 인터페이스(보트 등) 아웃라인 끄기
		if (HoveredActor)
		{
			if (IMagicInteractableInterface* Interface = Cast<IMagicInteractableInterface>(HoveredActor))
			{
				Interface->SetMageHighlight(false);
			}
			HoveredActor = nullptr;
		}

		return; // 여기서 함수 종료 (LineTrace 실행 안 함)
	}

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
	bool bIsInterface = (HitActor && HitActor->Implements<UMagicInteractableInterface>());

	// A. 일반 기둥
	if (HitPillar && !HitPillar->bIsFallen)
	{
		if (FocusedPillar != HitPillar)
		{
			// 이전 하이라이트 끄기
			if (HoveredActor)
			{
				if (IMagicInteractableInterface* Interface = Cast<IMagicInteractableInterface>(HoveredActor))
				{
					Interface->SetMageHighlight(false);
				}
				HoveredActor = nullptr;
			}
			if (FocusedPillar) FocusedPillar->PillarMesh->SetRenderCustomDepth(false);

			// 새 기둥 켜기
			HitPillar->PillarMesh->SetRenderCustomDepth(true);
			HitPillar->PillarMesh->SetCustomDepthStencilValue(251);
			FocusedPillar = HitPillar;
		}
	}
	// B. 인터페이스 액터 처리
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
				{
					Iface->SetMageHighlight(false);
				}
			}
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
		if (HoveredActor)
		{
			if (IMagicInteractableInterface* Interface = Cast<IMagicInteractableInterface>(HoveredActor))
			{
				Interface->SetMageHighlight(false);
			}
			HoveredActor = nullptr;
		}
	}
}

void AMageCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookInput = Value.Get<FVector2D>();

	// 1. 인터페이스 조종 모드 (보트 + 회전기둥)
	if (bIsBoatMode && CurrentControlledActor)
	{
		FVector MoveDir = FVector::ZeroVector;

		// A. 보트일 때 (카메라 기준 이동 방향 계산)
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
		// B. 그 외 (회전 기둥 등): 마우스 Y 움직임을 전달
		else
		{
			// 감도 조절
			if (FMath::Abs(LookInput.Y) > 0.1f)
			{
				MoveDir = FVector(0.f, LookInput.Y, 0.f);
			}
		}

		// [공통] 인터페이스를 통해 입력 전달
		if (!MoveDir.IsNearlyZero())
		{
			Server_InteractActor(CurrentControlledActor, MoveDir);
		}
	}
	// 2. 일반 기둥 모드 (Look 입력 무시, 제스처는 별도 로직)
	else if (bIsPillarMode)
	{
		if (IsValid(CurrentTargetPillar) && !CurrentTargetPillar->bIsFallen)
		{
			if (FMath::Abs(LookInput.X) > 0.5f && FMath::Sign(LookInput.X) == CurrentTargetPillar->
				RequiredMouseDirection)
			{
				Server_TriggerPillarFall(CurrentTargetPillar);
				EndJobAbility();
			}
		}
	}
	// 3. 일반 시점
	else
	{
		Super::Look(Value);
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

// ====================================================================================
//  섹션 4: 전투 및 스킬 시스템 (Combat & Skills)
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

void AMageCharacter::ProcessSkill(FName SkillRowName)
{
    if (!CanAttack()) return;

    // 캐시에서 데이터 꺼내오기
    FSkillData** FoundData = SkillDataCache.Find(SkillRowName);
    FSkillData* Data = (FoundData) ? *FoundData : nullptr;

    // 쿨타임 체크
    if (SkillTimers.Contains(SkillRowName))
    {
       if (GetWorldTimerManager().IsTimerActive(SkillTimers[SkillRowName]))
       {
          return;
       }
    }

    if (Data)
    {
       // 해금 여부 확인 (필수)
       if (!IsSkillUnlocked(Data->RequiredStage)) return;

       // 몽타주 재생 (클라/서버 공통)
       if (Data->SkillMontage) PlayActionMontage(Data->SkillMontage);

       if (HasAuthority())
       {
          if (Data->SkillMontage) Server_PlayMontage(Data->SkillMontage);

          // 평타 트레일
          if (SkillRowName == TEXT("Attack_A") || SkillRowName == TEXT("Attack_B"))
          {
             Multicast_SetTrailActive(true);
          }
          
          // =========================================================
          // Case A: 스킬 2 (유지형 이펙트 + 바위 소환)
          // =========================================================
          if (SkillRowName == TEXT("Skill2")) 
          {
             // 1. 캐스팅 이펙트 (서버에서만 보임 - 필요시 멀티캐스트로 변경 가능)
             if (Skill2CastEffect)
             {
             	// 1. 변수 먼저 선언
             	USceneComponent* AttachTarget = nullptr;

             	// 2. if문으로 각각 대입 (부모 클래스인 USceneComponent로 자동 형변환됨)
             	if (CachedWeaponMesh)
             	{
             		AttachTarget = CachedWeaponMesh;
             	}
             	else
             	{
             		AttachTarget = GetMesh();
             	}
                 
                Skill2CastComp = UGameplayStatics::SpawnEmitterAttached(
                   Skill2CastEffect,
                   AttachTarget,           
                   TEXT("Weapon_Root_R"),  
                   FVector::ZeroVector,
                   FRotator::ZeroRotator,
                   EAttachLocation::SnapToTarget,
                   true                    
                );
             }
             
             // 2. 바위 소환 타이머
             if (Data->ProjectileClass)
             {
                float Dmg = Data->Damage;
                FTimerDelegate TimerDel;
                TimerDel.BindUObject(this, &AMageCharacter::SpawnSkill2Rock, Data->ProjectileClass.Get(), Dmg);
                
                if (Data->ActionDelay > 0.0f)
                   GetWorldTimerManager().SetTimer(RockSpawnTimerHandle, TimerDel, Data->ActionDelay, false);
                else
                   SpawnSkill2Rock(Data->ProjectileClass, Dmg);
             }
          }
          // =========================================================
          // Case B: 스킬 1 & 평타 (즉발성 이펙트 + 투사체)
          // =========================================================
          else
          {
             // 스킬 1번이면 "쾅!" 하고 터지는 이펙트 즉시 실행
             if (SkillRowName == TEXT("Skill1"))
             {
                 Multicast_PlaySkill1VFX();
             }

             // 투사체 발사 로직 (기존 유지)
             if (Data->ProjectileClass)
             {
                float Dmg = Data->Damage;
                FTimerDelegate TimerDel;
                TimerDel.BindUObject(this, &AMageCharacter::SpawnDelayedProjectile, Data->ProjectileClass.Get(), Dmg);

                if (Data->ActionDelay > 0.0f)
                   GetWorldTimerManager().SetTimer(ProjectileTimerHandle, TimerDel, Data->ActionDelay, false);
                else
                   SpawnDelayedProjectile(Data->ProjectileClass, Dmg);
             }
          }
       }
       else
       {
          Server_ProcessSkill(SkillRowName);
       }

       // 쿨타임 등록 및 UI 갱신
       if (Data->Cooldown > 0.0f)
       {
          FTimerHandle& Handle = SkillTimers.FindOrAdd(SkillRowName);
          GetWorldTimerManager().SetTimer(Handle, Data->Cooldown, false);
          
          int32 SkillIdx = 0;
          if (SkillRowName == TEXT("Skill1")) SkillIdx = 1;
          else if (SkillRowName == TEXT("Skill2")) SkillIdx = 2;
          
          if (SkillIdx > 0)
          {
             TriggerSkillCooldown(SkillIdx, Data->Cooldown);
          }
       }
    }
}

void AMageCharacter::SpawnDelayedProjectile(UClass* ProjectileClass, float DamageAmount)
{
	if (!HasAuthority() || !ProjectileClass) return;

	Multicast_SetTrailActive(false);
	FVector SpawnLoc;
	FName SocketName = TEXT("Weapon_Root_R");

	if (GetMesh() && GetMesh()->DoesSocketExist(SocketName))
		SpawnLoc = GetMesh()->GetSocketLocation(SocketName);
	else
		SpawnLoc = GetActorLocation() + (GetActorForwardVector() * 100.f);

	FRotator SpawnRot = GetActorRotation();

	// 화면 중앙 에임 계산
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		FVector CamLoc;
		FRotator CamRot;
		PC->GetPlayerViewPoint(CamLoc, CamRot);

		FVector TraceStart = CamLoc;
		FVector TraceEnd = CamLoc + (CamRot.Vector() * 5000.0f);

		FHitResult HitResult;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);

		bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, Params);
		FVector TargetPoint = bHit ? HitResult.ImpactPoint : TraceEnd;

		SpawnRot = UKismetMathLibrary::FindLookAtRotation(SpawnLoc, TargetPoint);
	}

	// 투사체 생성 및 데미지 전달
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetInstigator();

	AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(ProjectileClass, SpawnLoc, SpawnRot, SpawnParams);

	if (AMageProjectile* MyProjectile = Cast<AMageProjectile>(SpawnedActor))
	{
		MyProjectile->Damage = DamageAmount;
	}
}

// ====================================================================================
//  섹션 5: 직업 능력 (Job Ability - Telekinesis/Boat) 및 스킬
// ====================================================================================

void AMageCharacter::JobAbility()
{
	if (!CanAttack()) return;
	if (!IsSkillUnlocked(1)) return;

	if (bIsDead) return;

	UAnimMontage* TargetMontage = nullptr;

	// [최적화] 캐시에서 조회
	FSkillData** FoundData = SkillDataCache.Find(FName("JobAbility"));
	FSkillData* Data = (FoundData) ? *FoundData : nullptr;

	if (Data)
	{
		TargetMontage = Data->SkillMontage;
	}
	
	bool bSuccess = false;
	
	// Case 1: 일반 기둥 모드 진입
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
	// Case 2: 인터페이스 액터 (보트 OR 회전기둥)
	else if (HoveredActor)
	{
		AMagicBoat* TargetBoat = Cast<AMagicBoat>(HoveredActor);

		if (TargetMontage)
		{
			PlayActionMontage(TargetMontage);
			Server_PlayMontage(TargetMontage);
		}

		// 보트 탑승 체크
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
			// MovePillar는 우클릭 즉시 반응해야 하므로 강제 신호 발송
			Server_InteractActor(CurrentControlledActor, FVector(1.0f, 0.0f, 0.0f));
		}

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
	
	
	if (bSuccess)
	{
		Server_SetAuraActive(true);
	}
	
}

void AMageCharacter::EndJobAbility()
{
	UAnimMontage* TargetMontage = nullptr;

	// [최적화] 캐시에서 조회
	FSkillData** FoundData = SkillDataCache.Find(FName("JobAbility"));
	FSkillData* Data = (FoundData) ? *FoundData : nullptr;

	if (Data)
	{
		TargetMontage = Data->SkillMontage;
	}

	if (TargetMontage)
	{
		StopAnimMontage(TargetMontage);
		Server_StopMontage(TargetMontage);
	}

	Server_SetAuraActive(false);
	
	// Case 1: 기둥 모드 종료
	if (bIsPillarMode)
	{
		bIsPillarMode = false;
		CurrentTargetPillar = nullptr;
		if (CameraTimelineComp) CameraTimelineComp->Reverse();
	}

	// Case 2: 인터페이스 모드 종료
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

	// 공통 복구
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
	if (Skill2CastComp)
	{
		Skill2CastComp->DeactivateSystem(); // 즉시 끄기
		Skill2CastComp = nullptr; // 안전하게 초기화
	}
	// 서버만 실행 & 클래스 체크
	if (!HasAuthority() || !RockClass) return;
	
	// 1. 소환 거리
	float SpawnDistance = 600.0f;
	
	// 2. 위치 계산 : (내 위치) + (앞쪽 방향 * 거리) + (위로 살짝 띄움)
	FVector ActorLoc = GetActorLocation();
	FVector ForwardDir = GetActorForwardVector();
	
	// 레이저 시작점 (플레이어 전방 6m 지점의 공중)
	FVector TraceStart = ActorLoc + (ForwardDir * SpawnDistance) + FVector (0.f,0.f, 200.f);
	// 레이저 끝점 (바닥을 찾기 위해 아래로 쏨)
	FVector TraceEnd = TraceStart - FVector (0.f,0.f, 1000.f);
	
	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	
	FVector FinalSpawnLoc = TraceStart;
	
	// 3. 바닥 찾기
	if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		FinalSpawnLoc = HitResult.ImpactPoint; // 바닥 위치 발견
	}
	else
	{
		// 바닥이 없으면(허공이면) 플레이어 높이 기준으로 설정
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red,TEXT("Skill Failed: No Ground Found!"));
		return;
	}
	
	FRotator SpawnRot = FRotator::ZeroRotator;
	
	// 4. 액터(바위) 생성
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	
	AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(RockClass, FinalSpawnLoc,
		SpawnRot, SpawnParams);
	
	// 5. 초기화
	if (AMageSkill2Rock* Rock = Cast<AMageSkill2Rock>(SpawnedActor))
	{
		Rock-> InitializeRock(DamageAmount, this);
	}
}

// ====================================================================================
//  섹션 7: 네트워크 및 유틸리티 (Network RPCs & Utils)
// ====================================================================================

void AMageCharacter::Server_ProcessSkill_Implementation(FName SkillRowName)
{
	ProcessSkill(SkillRowName);
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

void AMageCharacter::Server_StopMontage_Implementation(UAnimMontage* MontageToStop)
{
	Multicast_StopMontage(MontageToStop);
}

void AMageCharacter::Multicast_StopMontage_Implementation(UAnimMontage* MontageToStop)
{
	if (MontageToStop && !IsLocallyControlled())
	{
		StopAnimMontage(MontageToStop);
	}
}

void AMageCharacter::Server_InteractActor_Implementation(AActor* TargetActor, FVector Direction)
{
	if (TargetActor)
	{
		IMagicInteractableInterface* Interface = Cast<IMagicInteractableInterface>(TargetActor);
		if (Interface)
		{
			Interface->ProcessMageInput(Direction);
		}
	}
}

void AMageCharacter::Server_TriggerPillarFall_Implementation(APillar* TargetPillar)
{
	if (TargetPillar) TargetPillar->TriggerFall();
}

void AMageCharacter::Server_SetBoatRideState_Implementation(AMagicBoat* Boat, bool bRiding)
{
	if (Boat)
	{
		Boat->SetRideState(bRiding);
	}
}


void AMageCharacter::Multicast_SetTrailActive_Implementation(bool bActive)
{
	if (StaffTrailComp)
	{
		if (bActive)
		{
			StaffTrailComp->Activate(true);
		}
		else
		{
			StaffTrailComp->Deactivate();
		}
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
		if (bActive)
		{
			JobAuraComp->Activate(true); // 켜기 (Reset)
		}
		else
		{
			JobAuraComp->Deactivate(); // 끄기
		}
	}
}

// MageCharacter.cpp

void AMageCharacter::Multicast_PlaySkill1VFX_Implementation()
{
	if (Skill1CastEffect)
	{
		// 지팡이 끝(혹은 손)에 파티클 부착하여 재생
		// Weapon_Root_R 소켓을 사용하거나, TipSocket이 있다면 그걸 사용
		// 1. 변수 먼저 선언
		USceneComponent* AttachTarget = nullptr;

		// 2. if문으로 각각 대입 (부모 클래스인 USceneComponent로 자동 형변환됨)
		if (CachedWeaponMesh)
		{
			AttachTarget = CachedWeaponMesh;
		}
		else
		{
			AttachTarget = GetMesh();
		}
		FName SocketName = TEXT("Weapon_Root_R"); 

		

		UGameplayStatics::SpawnEmitterAttached(
			Skill1CastEffect,
			AttachTarget,
			SocketName,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			true // AutoDestroy (재생 끝나면 자동 삭제)
		);
	}
}