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

	// 컨트롤러 회전 설정 (마우스 돌릴 때 캐릭터 몸통도 같이 돌지 여부)
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	// 이동 컴포넌트 설정
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = 550.f;
		MoveComp->bOrientRotationToMovement = false;
		MoveComp->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	}

	AttackCooldownTime = 1.f;
	UnlockedStageLevel = 1;

	// 기둥/타겟 초기화
	FocusedPillar = nullptr;
	CurrentTargetPillar = nullptr;

	// [기둥용] 타임라인 컴포넌트
	CameraTimelineComp = CreateDefaultSubobject<UTimelineComponent>(TEXT("CameraTimelineComp"));

	// [보트용] 탑다운 카메라 컴포넌트 (스프링암 + 카메라)
	TopDownSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("TopDownSpringArm"));
	TopDownSpringArm->SetupAttachment(GetRootComponent());
	TopDownSpringArm->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f)); // 수직으로 내려다보기
	TopDownSpringArm->TargetArmLength = 1200.0f;
	TopDownSpringArm->bDoCollisionTest = false;

	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCamera->SetupAttachment(TopDownSpringArm, USpringArmComponent::SocketName);
	TopDownCamera->SetAutoActivate(false);

	// 기본(상속받은) 스프링암 설정
	if (SpringArm)
	{
		SpringArm->TargetOffset = FVector(0.0f, 0.0f, 150.0f);
		SpringArm->ProbeSize = 12.0f;
		SpringArm->bUsePawnControlRotation = true;
	}

	// [직업 아우라] 이펙트 컴포넌트
	JobAuraComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("JobAuraComp"));
	// 일단 골반 쪽에 붙여둠 (나중에 켜짐)
	JobAuraComp->SetupAttachment(GetMesh(), TEXT("Bip001-Pelvis"));
	JobAuraComp->bAutoActivate = false;

	// =========================================================
	//  [핵심] 지팡이 및 악세서리 부착 설정 (여기가 중요합니다!)
	// =========================================================

	// 1. 지팡이 컴포넌트 생성 (이름: Weapon_Root_R)
	WeaponRootComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Weapon_Root_R"));
	// 실제 캐릭터 메쉬의 손/지팡이 소켓에 부착
	WeaponRootComp->SetupAttachment(GetMesh(), TEXT("Weapon_Root_R"));

	// 2. [BaseCharacter 수정사항 적용] 
	// "상점 아이템을 장착할 때, 몸통(Mesh)이 아니라 내 지팡이(WeaponRootComp)에 붙여라" 라고 지정
	ItemAttachParent = WeaponRootComp;

	// 3. 소켓 이름도 지팡이 소켓 이름과 맞춰줌 (혹시 몰라서 동기화)
	AccessorySocketName = FName("Weapon_Root_R");

	// 4. 부모 클래스에서 만들어진 악세서리 컴포넌트들을 지팡이 아래로 이동시킴
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

	if (IsLocallyControlled())
	{
		GetWorldTimerManager().SetTimer(InteractionTimerHandle, this, &AMageCharacter::CheckInteractableTarget, 0.1f, true);
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
	if (!IsLocallyControlled())
	{
		// 혹시라도 타이머가 돌고 있다면 강제로 꺼버림 (확실한 사살)
		GetWorldTimerManager().ClearTimer(InteractionTimerHandle);
		return;
	}

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
			HitPillar->PillarMesh->SetCustomDepthStencilValue(255);
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

void AMageCharacter::ProcessSkill(FName SkillRowName, FVector TargetLocation)
{
    if (!CanAttack()) return;

    // 🔥 [핵심 픽스] 내가 직접 조종 중인 캐릭터라면(호스트든 클라든), 무조건 크로스헤어 조준점을 계산합니다!
    if (TargetLocation.IsZero() && IsLocallyControlled())
    {
        TargetLocation = GetCrosshairTargetLocation();
    }

    // 캐시에서 데이터 꺼내오기
    FSkillData** FoundData = SkillDataCache.Find(SkillRowName);
    FSkillData* Data = (FoundData) ? *FoundData : nullptr;

    // 쿨타임 체크
    if (SkillTimers.Contains(SkillRowName))
    {
       if (GetWorldTimerManager().IsTimerActive(SkillTimers[SkillRowName])) return;
    }

    if (Data)
    {
       if (!IsSkillUnlocked(Data->RequiredStage)) return;

       if (Data->SkillMontage) PlayActionMontage(Data->SkillMontage);

       if (HasAuthority()) // 서버(방장)일 때
       {
          if (Data->SkillMontage) Server_PlayMontage(Data->SkillMontage);

          if (SkillRowName == TEXT("Attack_A") || SkillRowName == TEXT("Attack_B"))
          {
             Multicast_SetTrailActive(true);
          }
          
          if (SkillRowName == TEXT("Skill2")) 
          {
             if (Skill2CastEffect)
             {
                USceneComponent* AttachTarget = CachedWeaponMesh ? (USceneComponent*)CachedWeaponMesh : (USceneComponent*)GetMesh();
                Skill2CastComp = UGameplayStatics::SpawnEmitterAttached(
                   Skill2CastEffect, AttachTarget, TEXT("Weapon_Root_R"),  
                   FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
             }
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
          else // 스킬 1 & 평타 (투사체 발사)
          {
             if (SkillRowName == TEXT("Skill1")) Multicast_PlaySkill1VFX();

             if (Data->ProjectileClass)
             {
                float Dmg = Data->Damage;
                
                // 위에서 완벽하게 계산된 TargetLocation을 넘겨줍니다!
                FTimerDelegate TimerDel;
                TimerDel.BindUObject(this, &AMageCharacter::SpawnDelayedProjectile, Data->ProjectileClass.Get(), Dmg, TargetLocation);

                if (Data->ActionDelay > 0.0f)
                   GetWorldTimerManager().SetTimer(ProjectileTimerHandle, TimerDel, Data->ActionDelay, false);
                else
                   SpawnDelayedProjectile(Data->ProjectileClass, Dmg, TargetLocation);
             }
          }
       }
       else // 클라이언트일 때
       {
          // 위에서 계산한 좌표를 서버로 전송
          Server_ProcessSkill(SkillRowName, TargetLocation);
       }

       // 쿨타임 등록
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

// MageCharacter.cpp

void AMageCharacter::SpawnDelayedProjectile(UClass* ProjectileClass, float DamageAmount, FVector TargetLocation)
{
	if (!HasAuthority() || !ProjectileClass) return;

	Multicast_SetTrailActive(false);

	// 1. 발사 시작 위치 (지팡이 끝)
	FVector SpawnLoc;
	FName SocketName = TEXT("Weapon_Root_R");

	if (GetMesh() && GetMesh()->DoesSocketExist(SocketName))
		SpawnLoc = GetMesh()->GetSocketLocation(SocketName);
	else
		SpawnLoc = GetActorLocation() + (GetActorForwardVector() * 100.f);

	FRotator SpawnRot;

	// 🔥 [핵심 픽스] 발사 지점과 타겟이 가깝다고 씹어버리는 방어 코드를 싹 지웠습니다!
	if (TargetLocation.IsZero())
	{
		// 타겟 좌표가 아예 없을 때만 기본 회전 적용
		SpawnRot = GetActorRotation();
	}
	else
	{
		// 거리가 가깝든 멀든, 무조건 타겟 위치를 향해 각도를 꺾어버림!
		SpawnRot = UKismetMathLibrary::FindLookAtRotation(SpawnLoc, TargetLocation);
	}

	// 3. 생성
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetInstigator();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(ProjectileClass, SpawnLoc, SpawnRot, SpawnParams);

	if (AMageProjectile* MyProjectile = Cast<AMageProjectile>(SpawnedActor))
	{
		MyProjectile->Damage = DamageAmount;
		MyProjectile->SetOwner(this); 
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

void AMageCharacter::Server_ProcessSkill_Implementation(FName SkillRowName,FVector TargetLocation)
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

float AMageCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// 1. 부모 클래스 로직 실행 (체력 감소 등)
	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// 2. 직업 능력(기둥/보트) 사용 중 피격 시 취소 로직
	if (ActualDamage > 0.0f && (bIsPillarMode || bIsBoatMode))
	{
		EndJobAbility();
	}

	return ActualDamage;
}

FVector AMageCharacter::GetCrosshairTargetLocation()
{
	// 1. 카메라 컴포넌트의 실제 위치와 보는 방향을 '직접' 가져옵니다.
	if (!Camera) return GetActorLocation() + GetActorForwardVector() * 1000.f;

	FVector CamLoc = Camera->GetComponentLocation();
	FVector CamForward = Camera->GetForwardVector();

	FVector TraceStart = CamLoc;
	FVector TraceEnd = CamLoc + (CamForward * 10000.0f); // 약 100미터 앞

	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this); // 자기 자신(마법사)은 무시

	// 2. 화면 중앙(카메라 전방)으로 레이저 쏘기
	bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, Params);

	// 3. 부딪힌 곳이 있으면 거기로, 없으면 허공 끝점으로 반환
	return bHit ? HitResult.ImpactPoint : TraceEnd;
}