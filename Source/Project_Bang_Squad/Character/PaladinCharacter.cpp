
#include "Project_Bang_Squad/Character/PaladinCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/DataTable.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"


APaladinCharacter::APaladinCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	
	bReplicates = true;
	SetReplicateMovement(true);
	
	// 1. 팔라딘 무빙 (묵직 고려)
	GetCharacterMovement()->MaxWalkSpeed = 450.f;
	GetCharacterMovement()->JumpZVelocity = 550.f;
	GetCharacterMovement()->AirControl = 0.35f;
	
	// 2. 공격 쿨타임 (모션 길이에 맞춰 조절)
	AttackCooldownTime = 1.f;
	
	// 3. 스트레이핑 모드 (카메라 보는 방향 유지)
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	
	// 4. 카메라 
	if (SpringArm) 
	{
		SpringArm->TargetOffset = FVector(0.0f, 0.0f, 150.0f); 
		SpringArm->ProbeSize = 12.0f; 
	}
}

void APaladinCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void APaladinCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	
	// 입력 바인딩
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// 점프
		if (JumpAction)
		{
			EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
			EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		}
		if (AttackAction)
		{
			EIC->BindAction(AttackAction, ETriggerEvent::Started, this, &APaladinCharacter::Attack);
		}
	}
}

void APaladinCharacter::Attack()
{
	// 쿨타임 체크
	if (!CanAttack()) return;
	
	// 쿨타임 시작 
	StartAttackCooldown();
	
	// 콤보 이름 결정
	FName SkillName;
	if (CurrentComboIndex == 0) SkillName = TEXT("Attack_A");
	else SkillName = TEXT("Attack_B");
	
	// 스킬 실행
	ProcessSkill(SkillName);
	
	// 콤보 인덱스 중가
	CurrentComboIndex++;
	if (CurrentComboIndex > 1) CurrentComboIndex = 0;
	
	// 리셋 타이머
	GetWorldTimerManager().ClearTimer(ComboResetTimer);
	GetWorldTimerManager().SetTimer(ComboResetTimer, this, &APaladinCharacter::ResetCombo, 1.5f, false);
}


void APaladinCharacter::ResetCombo()
{
	CurrentComboIndex = 0;
}

// 데이터 테이블 처리
void APaladinCharacter::ProcessSkill(FName SkillRowName)
{
	if (!SkillDataTable) return;
	static const FString ContextString(TEXT("PaladinSkillContext"));
	FSkillData* Data = SkillDataTable->FindRow<FSkillData>(SkillRowName, ContextString);
	
	if (Data)
	{
		// 레벨 제한 체크
		if (!IsSkillUnlocked(Data->RequiredStage)) return;
		
		CurrentSkillDamage = Data->Damage;
		
		// 몽타주 재생
		if (Data->SkillMontage)
		{
			PlayAnimMontage(Data->SkillMontage);
			Server_PlayMontage(Data->SkillMontage);
		}
	}
	if (HasAuthority())
	{
		// 기존 타이머 초기화 (연타 시 꼬임 방지)
		GetWorldTimerManager().ClearTimer(AttackHitTimerHandle);

		float HitDelay = Data->ActionDelay; // 데이터 테이블의 시간값 사용

		// 딜레이가 있으면 타이머 설정, 없으면(0.0) 즉시 타격
		if (HitDelay > 0.0f)
		{
			GetWorldTimerManager().SetTimer(
				AttackHitTimerHandle,
				this,
				&APaladinCharacter::ExecuteMeleeHit, // 시간이 되면 이 함수 실행
				HitDelay,
				false
			);
		}
		else
		{
			ExecuteMeleeHit();
		}
	}
}

// 서버 RPC (애니메이션 동기화)
void APaladinCharacter::Server_PlayMontage_Implementation(UAnimMontage* MontageToPlay)
{
	Multicast_PlayMontage(MontageToPlay);
}

void APaladinCharacter::Multicast_PlayMontage_Implementation(UAnimMontage* MontageToPlay)
{
	if (MontageToPlay && !IsLocallyControlled())
	{
		PlayAnimMontage(MontageToPlay);
	}
}

// PaladinCharacter.cpp

void APaladinCharacter::ExecuteMeleeHit()
{
	if (!HasAuthority()) return;

	// 1. 박스의 중심점 계산 (내 위치에서 앞으로 절반만큼 나간 곳)
	// 이유: 박스는 중심을 기준으로 그려지므로, 사거리의 절반만큼 이동해야 내 바로 앞에서 시작됨.
	FVector CenterLoc = GetActorLocation() + (GetActorForwardVector() * (AttackRange * 0.5f));

	// 2. 충돌 검사 (Box 형태)
	TArray<FHitResult> HitResults;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	// Box 모양 만들기 (절반 크기를 넣어야 함 - Extent 개념)
	// X: 앞뒤 길이의 절반, Y: 좌우 너비의 절반, Z: 높이의 절반
	FCollisionShape BoxShape = FCollisionShape::MakeBox(FVector(AttackRange * 0.5f, AttackWidth, AttackHeight));

	// 3. 내 회전(Rotation)을 적용해서 박스도 같이 돌려줌
	bool bHit = GetWorld()->SweepMultiByChannel(
		HitResults,
		CenterLoc,
		CenterLoc, // 시작과 끝이 같음 (움직이는 게 아니라 그 자리에 펑 하고 소환)
		GetActorQuat(), // 내 캐릭터가 보는 방향대로 박스 회전
		ECC_Pawn,
		BoxShape,
		Params
	);

	// [디버그] 박스 크기 눈으로 확인 (보라색 박스)
	// DrawDebugBox(GetWorld(), CenterLoc, BoxShape.GetExtent(), GetActorQuat(), FColor::Purple, false, 1.0f);

	if (bHit)
	{
		TSet<AActor*> DamagedActors; // 중복 타격 방지

		for (const FHitResult& Hit : HitResults)
		{
			AActor* HitActor = Hit.GetActor();
			if (HitActor && HitActor != this && !DamagedActors.Contains(HitActor))
			{
				UGameplayStatics::ApplyDamage(
					HitActor,
					CurrentSkillDamage,
					GetController(), 
					this,            
					UDamageType::StaticClass()
				);
				DamagedActors.Add(HitActor);
			}
		}
	}
}
